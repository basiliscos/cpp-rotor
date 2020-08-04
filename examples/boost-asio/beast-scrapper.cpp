//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

/*

    The current example in some extent mimics curl usage example in CAF
( https://github.com/actor-framework/actor-framework/blob/master/examples/curl/curl_fuse.cpp ),
but instead of CAF + curl pair, the rotor + bease pair is used.
There is a single client (can be multiple), there is a single http-manager (supervisor),
which holds a pool of multiple http-workers.

client -> (timerC_1) request_1 -> http-manager -> (timerM_1) request_1 -> http-worker_1
          (timerC_2) request_2 ->              -> (timerM_2) request_1 -> http-worker_2
                      ...                                      ...          ...
          (timerC_n) request_n ->              -> (timerM_n) request_1 -> http-worker_n

1. The client makes an request, which contains the URL of the remote resource, and the buffer
where the parsed reply is put.

2. The reply is intrusive pointer to the real reply, i.e. to avoid unnecessary copying.

3. The client makes as many requests as it needs, and the manager just for forwards them
to free http-workers. The amounts of simultaneous requests and http-workers are "coordinated",
as well as MAX_FAILURES = 1. If it is not desirable, it can be impoved:

3.1. If MAX_FAILURES != 1, then it might be the case when timeout timer just has triggered
on the client, but corresponding timer wasn't triggered on the manager. In the current code
there is just an assert, but the situation where client asks more then http-manager is
capable to serve can be handled, i.e. via queueing.

3.2. If the pool size and client's simultaneous requests are uncoordinated, then http-manager
can queue requests. However, some *back-pressure* mechanisms should be imposed into http-manager
to prevent the queue to grow infinitely.

3.3. The http-requests are stateless (i.e. no http/1.1). This cannot be improved whitout
internal protocol change, as the socket should not be closed, the same http-client
should continue serving the requests on the same host from the client etc.

3.4. The http-requests do not contain headers/cookies etc. It can be improved via additional
fields in the http-requests.

3.5. There is no cancellation facilities. Again, the protocol should be changed to support
that: client have to know *what* to cancel (i.e. some work_id/request_id should be returned
immediately from http-worker to http-client). As the result in the current code the
client *have to wait* until all requests will be finished either successfully or via
timeout triggering. The noticible delay in shutdown can be observed, and it is described
here.

4. The care should be taken to properly shut down the application: only when there is
no I/O activities from client perspective, the shut down is initiated.

5. There are 2 timers per request: the 1st one validates manager's responsibilities
on client, the 2nd one validates worker's responsibilities on manager.
Technically, there could be only one timer per request, however, the main question
is how reliable do you treat your actors. If they are unreliable (i.e. might have
bugs etc.), then there should be 2 timers. On the later stage of the development,
it might be switched to one timer per request if reliability has been proven
and it is desirable to get rid of additional timer from performance point of view.

6. There should be no crashes, no memory leaks

The output sample for the localhost is:

./beast-scrapper --workers_count=50 --timeout=5000 --max_requests=50000 --url=http://127.0.0.1:80/index.html
using 50 workers for 127.0.0.1:80/index.html, timeout: 00:00:05
starting shutdown
client_t::shutdown_start
client_t::shutdown_finish, stats: 50000/50000 requests, in 2.96149s, rps = 16883.4
client_t::shutdown_finish
http_manager_t::shutdown_finish()

 */

#include "rotor.hpp"
#include "rotor/asio.hpp"
#include <iostream>
#include <chrono>
#include <regex>
#include <unordered_set>
#include <unordered_map>
#include <deque>
#include <memory>
#include <boost/program_options.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/lexical_cast.hpp>

namespace po = boost::program_options;
namespace http = boost::beast::http;
namespace sys = boost::system;
namespace asio = boost::asio;
namespace pt = boost::posix_time;
namespace ra = rotor::asio;
namespace r = rotor;
using tcp = asio::ip::tcp;

struct URL {
    std::string host;
    std::string port;
    std::string path;
};

using raw_http_response_t = http::response<http::string_body>;

constexpr const std::uint32_t RX_BUFF_SZ = 10 * 1024;
constexpr const std::uint32_t MAX_HTTP_FAILURES = 1;

struct endpoint_t {
    std::string host;
    std::string port;
    inline bool operator==(const endpoint_t &other) const noexcept { return host == other.host && port == other.port; }
};

namespace std {
template <> struct hash<endpoint_t> {
    inline size_t operator()(const endpoint_t &endpoint) const noexcept {
        auto h1 = std::hash<std::string>()(endpoint.host);
        auto h2 = std::hash<std::string>()(endpoint.port);
        return h1 ^ (h2 << 4);
    }
};

} // namespace std

namespace payload {

struct http_response_t : public r::arc_base_t<http_response_t> {
    raw_http_response_t response;
    std::size_t bytes;

    http_response_t(raw_http_response_t &&response_, std::size_t bytes_)
        : response(std::move(response_)), bytes{bytes_} {}
};

struct http_request_t : public r::arc_base_t<http_request_t> {
    using rx_buff_t = boost::beast::flat_buffer;
    using rx_buff_ptr_t = std::shared_ptr<rx_buff_t>;
    using duration_t = r::pt::time_duration;
    using response_t = r::intrusive_ptr_t<http_response_t>;

    http_request_t(const URL &url_, rx_buff_ptr_t rx_buff_, std::size_t rx_buff_size_, duration_t timeout_)
        : url{url_}, rx_buff{rx_buff_}, rx_buff_size{rx_buff_size_}, timeout{timeout_} {}
    URL url;
    rx_buff_ptr_t rx_buff;
    std::size_t rx_buff_size;
    duration_t timeout;
};

struct address_response_t : public r::arc_base_t<address_response_t> {
    using resolve_results_t = tcp::resolver::results_type;

    explicit address_response_t(resolve_results_t results_) : results{results_} {};
    resolve_results_t results;
};

struct address_request_t : public r::arc_base_t<address_request_t> {
    using response_t = r::intrusive_ptr_t<address_response_t>;
    endpoint_t endpoint;
    explicit address_request_t(const endpoint_t &endpoint_) : endpoint{endpoint_} {}
};

} // namespace payload

namespace message {
using http_request_t = r::request_traits_t<payload::http_request_t>::request::message_t;
using http_response_t = r::request_traits_t<payload::http_request_t>::response::message_t;
using resolve_request_t = r::request_traits_t<payload::address_request_t>::request::message_t;
using resolve_response_t = r::request_traits_t<payload::address_request_t>::response::message_t;
} // namespace message

namespace resource {
static const constexpr r::plugin::resource_id_t timer = 0;
static const constexpr r::plugin::resource_id_t io = 1;
} // namespace resource

namespace service {
static const char *resolver = "service:resolver";
static const char *manager = "service:manager";
} // namespace service

static_assert(r::details::is_constructible_v<payload::http_response_t, raw_http_response_t, std::size_t>, "valid");
static_assert(std::is_constructible_v<payload::http_response_t, raw_http_response_t, std::size_t>, "valid");

struct resolver_worker_config_t : public r::actor_config_t {
    r::pt::time_duration resolve_timeout;
    using r::actor_config_t::actor_config_t;
};

struct http_worker_config_t : public r::actor_config_t {
    r::pt::time_duration resolve_timeout;
    r::pt::time_duration request_timeout;
    using r::actor_config_t::actor_config_t;
};

template <typename Actor> struct resolver_worker_config_builder_t : r::actor_config_builder_t<Actor> {
    using builder_t = typename Actor::template config_builder_t<Actor>;
    using parent_t = r::actor_config_builder_t<Actor>;
    using parent_t::parent_t;

    builder_t &&resolve_timeout(const pt::time_duration &value) &&noexcept {
        parent_t::config.resolve_timeout = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }
};

template <typename Actor> struct http_worker_config_builder_t : r::actor_config_builder_t<Actor> {
    using builder_t = typename Actor::template config_builder_t<Actor>;
    using parent_t = r::actor_config_builder_t<Actor>;
    using parent_t::parent_t;

    builder_t &&resolve_timeout(const pt::time_duration &value) &&noexcept {
        parent_t::config.resolve_timeout = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

    builder_t &&request_timeout(const pt::time_duration &value) &&noexcept {
        parent_t::config.request_timeout = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }
};

struct resolver_worker_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    using request_ptr_t = r::intrusive_ptr_t<message::resolve_request_t>;
    using resolve_results_t = payload::address_response_t::resolve_results_t;
    using Queue = std::list<request_ptr_t>;
    using Cache = std::unordered_map<endpoint_t, resolve_results_t>;

    using config_t = resolver_worker_config_t;
    template <typename Actor> using config_builder_t = resolver_worker_config_builder_t<Actor>;

    explicit resolver_worker_t(config_t &config)
        : r::actor_base_t{config}, io_timeout{config.resolve_timeout},
          strand{static_cast<ra::supervisor_asio_t *>(config.supervisor)->get_strand()}, backend{strand.context()},
          timer(strand.context()) {}

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>(
            [&](auto &p) { p.subscribe_actor(&resolver_worker_t::on_request); });
        plugin.with_casted<r::plugin::registry_plugin_t>(
            [&](auto &p) { p.register_name(service::resolver, get_address()); });
    }

    bool maybe_shutdown() noexcept {
        if (state == r::state_t::SHUTTING_DOWN) {
            auto ec = r::make_error_code(r::error_code_t::actor_not_linkable);
            for (auto &req : queue) {
                reply_with_error(*req, ec);
            }
            queue.clear();

            if (resources->has(resource::io)) {
                backend.cancel();
            }
            if (resources->has(resource::timer)) {
                backend.cancel();
            }
            return true;
        }
        return false;
    }

    bool cancel_timer() noexcept {
        sys::error_code ec;
        timer.cancel(ec);
        if (ec) {
            get_supervisor().do_shutdown();
        }
        return (bool)ec;
    }

    void on_request(message::resolve_request_t &req) noexcept {
        if (state == r::state_t::SHUTTING_DOWN) {
            auto ec = r::make_error_code(r::error_code_t::actor_not_linkable);
            reply_with_error(req, ec);
            return;
        }

        queue.emplace_back(&req);
        if (!resources->has(resource::io))
            process();
    }

    template <typename ReplyFn> void reply(const endpoint_t &endpoint, ReplyFn &&fn) noexcept {
        auto it = queue.begin();
        while (it != queue.end()) {
            auto &message_ptr = *it;
            if (message_ptr->payload.request_payload->endpoint == endpoint) {
                fn(*message_ptr);
                it = queue.erase(it);
            } else {
                ++it;
            }
        }
    }

    void mass_reply(const endpoint_t &endpoint, const resolve_results_t &results) noexcept {
        reply(endpoint, [&](auto &message) { reply_to(message, results); });
    }

    void mass_reply(const endpoint_t &endpoint, const std::error_code &ec) noexcept {
        reply(endpoint, [&](auto &message) { reply_with_error(message, ec); });
    }

    void process() noexcept {
        if (queue.empty())
            return;
        auto queue_it = queue.begin();
        auto endpoint = (*queue_it)->payload.request_payload->endpoint;
        auto cache_it = cache.find(endpoint);
        if (cache_it != cache.end()) {
            mass_reply(endpoint, cache_it->second);
        }
        if (queue.empty())
            return;
        resolve_start(*queue_it);
    }

    void resolve_start(request_ptr_t &req) noexcept {
        if (resources->has_any())
            return;
        if (maybe_shutdown())
            return;
        if (queue.empty())
            return;

        auto &endpoint = req->payload.request_payload->endpoint;
        auto fwd_resolver =
            ra::forwarder_t(*this, &resolver_worker_t::on_resolve, &resolver_worker_t::on_resolve_error);
        backend.async_resolve(endpoint.host, endpoint.port, std::move(fwd_resolver));
        resources->acquire(resource::io);

        timer.expires_from_now(io_timeout);
        auto fwd_timer =
            ra::forwarder_t(*this, &resolver_worker_t::on_timer_trigger, &resolver_worker_t::on_timer_error);
        timer.async_wait(std::move(fwd_timer));
        resources->acquire(resource::timer);
    }

    void on_resolve(resolve_results_t results) noexcept {
        resources->release(resource::io);
        auto &endpoint = queue.front()->payload.request_payload->endpoint;
        auto pair = cache.emplace(endpoint, results);
        auto &it = pair.first;
        mass_reply(it->first, it->second);
        cancel_timer();
    }

    void on_resolve_error(const sys::error_code &ec) noexcept {
        resources->release(resource::io);
        auto endpoint = queue.front()->payload.request_payload->endpoint;
        mass_reply(endpoint, ec);
        cancel_timer();
    }

    void on_timer_error(const sys::error_code &ec) noexcept {
        resources->release(resource::timer);
        if (ec != asio::error::operation_aborted) {
            return get_supervisor().do_shutdown();
        }
        if (!maybe_shutdown()) {
            process();
        }
    }

    void on_timer_trigger() noexcept {
        resources->release(resource::timer);
        // could be actually some other ec...
        auto ec = r::make_error_code(r::error_code_t::request_timeout);
        auto endpoint = queue.front()->payload.request_payload->endpoint;
        mass_reply(endpoint, ec);
        if (!maybe_shutdown()) {
            process();
        }
    }

    // cancel any pending async ops
    void shutdown_start() noexcept override {
        r::actor_base_t::shutdown_start();
        maybe_shutdown();
    }

    pt::time_duration io_timeout;
    asio::io_context::strand &strand;
    tcp::resolver backend;
    asio::deadline_timer timer;
    Queue queue;
    Cache cache;
};

struct http_worker_t : public r::actor_base_t {
    using request_ptr_t = r::intrusive_ptr_t<message::http_request_t>;
    using tcp_socket_ptr_t = std::unique_ptr<tcp::socket>;
    using resolve_it_t = payload::address_response_t::resolve_results_t::iterator;

    using config_t = http_worker_config_t;
    template <typename Actor> using config_builder_t = http_worker_config_builder_t<Actor>;

    explicit http_worker_t(config_t &config)
        : r::actor_base_t{config}, resolve_timeout(config.resolve_timeout), request_timeout(config.request_timeout),
          strand{static_cast<ra::supervisor_asio_t *>(config.supervisor)->get_strand()}, timer{strand.context()} {}

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
            p.subscribe_actor(&http_worker_t::on_request);
            p.subscribe_actor(&http_worker_t::on_resolve);
        });
        plugin.with_casted<r::plugin::registry_plugin_t>([&](auto &p) {
            p.discover_name(service::resolver, resolver).link(false, [&](auto &ec) {
                if (ec) {
                    std::cerr << "resolver not found :; " << ec.message() << "\n";
                }
            });
        });
    }

    bool maybe_shutdown() noexcept {
        if (state == r::state_t::SHUTTING_DOWN) {
            if (resources->has(resource::io)) {
                sock->cancel();
            }
            if (resources->has(resource::timer)) {
                timer.cancel();
            }
            return true;
        }
        return false;
    }

    void on_request(message::http_request_t &req) noexcept {
        assert(!orig_req);
        assert(!sock);
        orig_req.reset(&req);
        http_response.clear();
        auto &url = req.payload.request_payload->url;
        endpoint_t endpoint{url.host, url.port};
        request<payload::address_request_t>(resolver, std::move(endpoint)).send(resolve_timeout);
    }

    void on_resolve(message::resolve_response_t &res) noexcept {
        auto &ec = res.payload.ec;
        if (ec) {
            reply_with_error(*orig_req, ec);
            orig_req.reset();
            return;
        }

        sys::error_code ec_sock;
        sock = std::make_unique<tcp::socket>(strand.context());
        sock->open(tcp::v4(), ec_sock);
        if (ec_sock) {
            reply_with_error(*orig_req, ec_sock);
            orig_req.reset();
            return;
        }

        auto &addresses = res.payload.res->results;
        auto fwd_connect = ra::forwarder_t(*this, &http_worker_t::on_connect, &http_worker_t::on_tcp_error);
        asio::async_connect(*sock, addresses.begin(), addresses.end(), std::move(fwd_connect));
        resources->acquire(resource::io);

        timer.expires_from_now(request_timeout);
        auto fwd_timer = ra::forwarder_t(*this, &http_worker_t::on_timer_trigger, &http_worker_t::on_timer_error);
        timer.async_wait(std::move(fwd_timer));
        resources->acquire(resource::timer);
    }

    void on_connect(resolve_it_t) noexcept {
        if (!orig_req) {
            resources->release(resource::io);
            return;
        }
        if (maybe_shutdown()) {
            return;
        }

        auto &url = orig_req->payload.request_payload->url;
        http_request.method(http::verb::get);
        http_request.version(11);
        http_request.target(url.path);
        http_request.set(http::field::host, url.host);
        http_request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        auto fwd = ra::forwarder_t(*this, &http_worker_t::on_request_sent, &http_worker_t::on_tcp_error);
        http::async_write(*sock, http_request, std::move(fwd));
    }

    void on_request_sent(std::size_t /* bytes */) noexcept {
        if (!orig_req) {
            resources->release(resource::io);
            return;
        }
        if (maybe_shutdown())
            return;

        auto fwd = ra::forwarder_t(*this, &http_worker_t::on_request_read, &http_worker_t::on_tcp_error);

        auto &rx_buff = orig_req->payload.request_payload->rx_buff;
        rx_buff->prepare(orig_req->payload.request_payload->rx_buff_size);
        http::async_read(*sock, *rx_buff, http_response, std::move(fwd));
    }

    void on_request_read(std::size_t bytes) noexcept {
        response_size = bytes;

        sys::error_code ec;
        /* sock->cancel(); */
        sock->close(ec);
        if (ec) {
            // we are going to destroy socket anyway
            std::cout << "socket closing error: " << ec.message() << "\n";
        }
        sock.reset();
        resources->release(resource::io);
        cancel_timer();
        maybe_shutdown();
    }

    void on_tcp_error(const sys::error_code &ec) noexcept {
        resources->release(resource::io);
        if (!orig_req) {
            return;
        }

        reply_with_error(*orig_req, ec);
        orig_req.reset();
        cancel_timer();
    }

    void on_timer_error(const sys::error_code &ec) noexcept {
        resources->release(resource::timer);
        if (ec != asio::error::operation_aborted) {
            if (orig_req) {
                reply_with_error(*orig_req, ec);
                orig_req.reset();
            }
            return get_supervisor().do_shutdown();
        }

        assert(!resources->has_any());
        reply_to(*orig_req, std::move(http_response), response_size);
        orig_req.reset();
    }

    void on_timer_trigger() noexcept {
        resources->release(resource::timer);
        if (orig_req) {
            auto ec = r::make_error_code(r::error_code_t::request_timeout);
            reply_with_error(*orig_req, ec);
            orig_req.reset();
        }
        if (sock) {
            sock->cancel();
        }
    }

    bool cancel_timer() noexcept {
        sys::error_code ec;
        timer.cancel(ec);
        if (ec) {
            get_supervisor().do_shutdown();
        }
        return (bool)ec;
    }

    // cancel any pending async ops
    void shutdown_start() noexcept override {
        r::actor_base_t::shutdown_start();
        maybe_shutdown();
    }

    pt::time_duration resolve_timeout;
    pt::time_duration request_timeout;
    asio::io_context::strand &strand;
    asio::deadline_timer timer;
    r::address_ptr_t resolver;
    request_ptr_t orig_req;
    tcp_socket_ptr_t sock;
    http::request<http::empty_body> http_request;
    http::response<http::string_body> http_response;
    size_t response_size;
};

struct http_manager_config_t : public ra::supervisor_config_asio_t {
    std::size_t worker_count;
    r::pt::time_duration worker_timeout;
    r::pt::time_duration resolve_timeout;
    r::pt::time_duration request_timeout;

    using ra::supervisor_config_asio_t::supervisor_config_asio_t;
};

template <typename Supervisor> struct http_manager_asio_builder_t : ra::supervisor_config_asio_builder_t<Supervisor> {
    using builder_t = typename Supervisor::template config_builder_t<Supervisor>;
    using parent_t = ra::supervisor_config_asio_builder_t<Supervisor>;
    using parent_t::parent_t;
    constexpr static const std::uint32_t WORKERS = 1 << 3;
    constexpr static const std::uint32_t WORKER_TIMEOUT = 1 << 4;
    constexpr static const std::uint32_t REQUEST_TIMEOUT = 1 << 5;
    constexpr static const std::uint32_t RESOLVE_TIMEOUT = 1 << 6;
    constexpr static const std::uint32_t requirements_mask =
        parent_t::requirements_mask | WORKERS | WORKER_TIMEOUT | REQUEST_TIMEOUT | RESOLVE_TIMEOUT;

    builder_t &&workers(std::size_t count) && {
        parent_t::config.worker_count = count;
        parent_t::mask = (parent_t::mask & ~WORKERS);
        return std::move(*static_cast<builder_t *>(this));
    }

    builder_t &&worker_timeout(const r::pt::time_duration &value) && {
        parent_t::config.worker_timeout = value;
        parent_t::mask = (parent_t::mask & ~WORKER_TIMEOUT);
        return std::move(*static_cast<builder_t *>(this));
    }

    builder_t &&resolve_timeout(const r::pt::time_duration &value) && {
        parent_t::config.resolve_timeout = value;
        parent_t::mask = (parent_t::mask & ~RESOLVE_TIMEOUT);
        return std::move(*static_cast<builder_t *>(this));
    }

    builder_t &&request_timeout(const r::pt::time_duration &value) && {
        parent_t::config.request_timeout = value;
        parent_t::mask = (parent_t::mask & ~REQUEST_TIMEOUT);
        return std::move(*static_cast<builder_t *>(this));
    }
};

struct http_manager_t : public ra::supervisor_asio_t {
    using workers_set_t = std::unordered_set<r::address_ptr_t>;
    using request_ptr_t = r::intrusive_ptr_t<message::http_request_t>;
    using req_mapping_t = std::unordered_map<r::request_id_t, request_ptr_t>;

    using config_t = http_manager_config_t;
    template <typename Supervisor> using config_builder_t = http_manager_asio_builder_t<Supervisor>;

    explicit http_manager_t(http_manager_config_t &config_)
        : ra::supervisor_asio_t{config_}, worker_count{config_.worker_count}, worker_timeout{config_.worker_timeout},
          request_timeout{config_.request_timeout}, resolve_timeout{config_.resolve_timeout} {}

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
            p.subscribe_actor(&http_manager_t::on_request);
            p.subscribe_actor(&http_manager_t::on_response);
        });
        plugin.with_casted<r::plugin::child_manager_plugin_t>([this](auto &) {
            for (std::size_t i = 0; i < worker_count; ++i) {
                auto worker_address = create_actor<http_worker_t>()
                                          .timeout(worker_timeout)
                                          .resolve_timeout(resolve_timeout)
                                          .request_timeout(request_timeout)
                                          .finish()
                                          ->get_address();
                workers.emplace(std::move(worker_address));
            }
        });
        plugin.with_casted<r::plugin::registry_plugin_t>(
            [&](auto &p) { p.register_name(service::manager, get_address()); });
    }

    void shutdown_finish() noexcept override {
        std::cerr << "http_manager_t::shutdown_finish()\n";
        ra::supervisor_asio_t::shutdown_finish();
    }

    void on_request(message::http_request_t &req) noexcept {
        auto it = workers.begin();
        if (it == workers.end()) {
            std::abort();
        }
        auto worker_addr = *it;
        workers.erase(it);
        auto &payload = req.payload.request_payload;
        auto request_id = request<payload::http_request_t>(worker_addr, payload).send(payload->timeout);
        req_mapping.emplace(request_id, &req);
    }

    void on_response(message::http_response_t &res) noexcept {
        auto it = req_mapping.find(res.payload.request_id());
        auto worker_addr = res.payload.req->address;
        workers.emplace(std::move(worker_addr));
        reply_to(*it->second, res.payload.ec, std::move(res.payload.res));
        req_mapping.erase(it);
    }

    std::size_t worker_count;
    r::pt::time_duration worker_timeout;
    r::pt::time_duration request_timeout;
    r::pt::time_duration resolve_timeout;
    workers_set_t workers;
    req_mapping_t req_mapping;
};

struct client_t : r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    using timepoint_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&client_t::on_response); });
        plugin.with_casted<r::plugin::registry_plugin_t>([&](auto &p) {
            p.discover_name(service::manager, manager_addr, true).link(false, [&](auto &ec) {
                std::cerr << "manager has been found:" << ec.message() << "\n";
            });
        });
    }

    void shutdown_finish() noexcept override {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        r::actor_base_t::shutdown_finish();
        double rps = success_requests / diff.count();

        std::cerr << "client_t::shutdown_finish, stats: " << success_requests << "/" << total_requests
                  << " requests, in " << diff.count() << "s, rps = " << rps << "\n";
        // send<r::payload::shutdown_trigger_t>(supervisor->get_address(), manager_addr);
        manager_addr.reset();
        supervisor->do_shutdown(); /* trigger all system to shutdown */
    }

    void make_request() noexcept {
        if (!shutdown_request) {
            if (active_requests < concurrency && total_requests < max_requests) {
                auto rx_buff = std::make_shared<payload::http_request_t::rx_buff_t>();
                request<payload::http_request_t>(manager_addr, url, std::move(rx_buff), RX_BUFF_SZ, timeout)
                    .send(timeout);
                ++active_requests;
                ++total_requests;
            }
        }
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        start = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < concurrency; ++i) {
            make_request();
        }
    }

    void on_response(message::http_response_t &msg) noexcept {
        --active_requests;
        bool err = false;
        if (!msg.payload.ec) {
            auto &res = msg.payload.res->response;
            if (res.result() == http::status::ok) {
                // std::cerr << "." << std::flush;
                ++success_requests;
            } else {
                std::cerr << "http error: " << res.result_int() << "\n";
                err = true;
            }
        } else {
            std::cerr << "request error: " << msg.payload.ec.message() << "\n";
            err = true;
        }
        if (err) {
            ++http_errors;
        }

        if (http_errors < MAX_HTTP_FAILURES) {
            make_request();
        }
        if (active_requests == 0) {
            std::cerr << "starting shutdown\n";
            do_shutdown();
        }
    }

    std::size_t poll_attempts = 0;
    std::size_t http_errors = 0;
    std::size_t active_requests = 0;
    std::size_t success_requests = 0;
    std::size_t total_requests = 0;
    std::size_t max_requests = 0;
    r::address_ptr_t manager_addr;
    URL url;
    r::pt::time_duration timeout;
    std::size_t concurrency;
    timepoint_t start;
};

int main(int argc, char **argv) {
    using url_ptr_t = std::unique_ptr<URL>;
    // clang-format off
    /* parse command-line & config options */
    po::options_description cmdline_descr("Allowed options");
    cmdline_descr.add_options()
        ("help", "show this help message")
        ("url", po::value<std::string>()->default_value("http://www.example.com:80/index.html"), "URL to poll")
        ("workers_count", po::value<std::size_t>()->default_value(1), "concurrency (number of http workers)")
        ("timeout", po::value<std::size_t>()->default_value(5000), "generic timeout (in milliseconds)")
        ("resolve_timeout", po::value<std::size_t>()->default_value(1000), "resolve timeout (in milliseconds)")
        ("rotor_timeout", po::value<std::size_t>()->default_value(10), "rotor timeout (in milliseconds)")
        ("max_requests", po::value<std::size_t>()->default_value(100), "maximum amount of requests before shutting down");
    // clang-format on

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, cmdline_descr), vm);
    po::notify(vm);

    bool show_help = vm.count("help");
    if (show_help) {
        std::cout << cmdline_descr << "\n";
        return 1;
    }

    url_ptr_t url;
    auto url_str = vm["url"].as<std::string>();
    std::regex re("(\\w+)://([^/ :]+):(\\d+)(/.+)");
    std::smatch what;
    if (regex_match(url_str, what, re)) {
        auto host = std::string(what[2].first, what[2].second);
        auto port = std::string(what[3].first, what[3].second);
        auto path = std::string(what[4].first, what[4].second);
        url = std::make_unique<URL>(URL{std::move(host), std::move(port), std::move(path)});
    } else {
        std::cout << "wrong url format. It should be like http://www.example.com:80/index.html"
                  << "\n";
        return 1;
    }

    auto request_timeout = r::pt::milliseconds{vm["timeout"].as<std::size_t>()};
    auto resolve_timeout = r::pt::milliseconds{vm["resolve_timeout"].as<std::size_t>()};
    auto rotor_timeout = r::pt::milliseconds{vm["rotor_timeout"].as<std::size_t>()};
    auto workers_count = vm["workers_count"].as<std::size_t>();
    std::cerr << "using " << workers_count << " workers for " << url->host << ":" << url->port << url->path
              << ", timeout: " << request_timeout << "\n";
    asio::io_context io_context;

    auto system_context = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_context)};
    auto strand = std::make_shared<asio::io_context::strand>(io_context);
    auto man_timeout = request_timeout + (rotor_timeout * static_cast<int>(workers_count) * 2);
    auto sup = system_context->create_supervisor<ra::supervisor_asio_t>()
                   .timeout(man_timeout)
                   .strand(strand)
                   .create_registry()
                   .synchronize_start()
                   .finish();

    sup->create_actor<resolver_worker_t>()
        .timeout(resolve_timeout + rotor_timeout)
        .resolve_timeout(resolve_timeout)
        .finish();

    auto worker_timeout = request_timeout * 2;
    sup->create_actor<http_manager_t>()
        .strand(strand)
        .timeout(man_timeout)
        .workers(workers_count)
        .worker_timeout(worker_timeout)
        .request_timeout(request_timeout)
        .resolve_timeout(resolve_timeout + rotor_timeout * 2)
        .synchronize_start()
        .finish();

    auto client = sup->create_actor<client_t>().timeout(worker_timeout).finish();
    client->timeout = request_timeout + rotor_timeout * 2;
    client->concurrency = workers_count;
    client->url = *url;
    client->max_requests = vm["max_requests"].as<std::size_t>();

    sup->start();
    io_context.run();

    return 0;
}
