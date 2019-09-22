//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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

namespace payload {

struct http_response_t : public r::arc_base_t<http_response_t> {
    raw_http_response_t response;
    std::size_t bytes;

    http_response_t(raw_http_response_t &&response_, std::size_t bytes_)
        : response(std::move(response_)), bytes{bytes_} {}
};

struct http_request_t : public r::arc_base_t<http_response_t> {
    using rx_buff_t = boost::beast::flat_buffer;
    using rx_buff_ptr_t = std::shared_ptr<rx_buff_t>;
    using duration_t = r::pt::time_duration;
    using response_t = r::intrusive_ptr_t<http_response_t>;

    http_request_t(URL url_, rx_buff_ptr_t rx_buff_, std::size_t rx_buff_size_, duration_t timeout_)
        : url{url_}, rx_buff{rx_buff_}, rx_buff_size{rx_buff_size_}, timeout{timeout_} {}
    URL url;
    rx_buff_ptr_t rx_buff;
    std::size_t rx_buff_size;
    duration_t timeout;
};

} // namespace payload

namespace message {

using http_request_t = r::request_traits_t<payload::http_request_t>::request::message_t;
using http_response_t = r::request_traits_t<payload::http_request_t>::response::message_t;

} // namespace message

static_assert(r::details::is_constructible_v<payload::http_response_t, raw_http_response_t, std::size_t>, "zzz");
static_assert(std::is_constructible_v<payload::http_response_t, raw_http_response_t, std::size_t>, "zzz");

struct http_worker_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    using request_ptr_t = r::intrusive_ptr_t<message::http_request_t>;
    using tcp_socket_ptr_t = std::unique_ptr<tcp::socket>;
    using resolve_results_t = tcp::resolver::results_type;
    using resolve_it_t = resolve_results_t::iterator;

    explicit http_worker_t(r::supervisor_t &sup)
        : r::actor_base_t{sup}, strand{static_cast<ra::supervisor_asio_t &>(sup).get_strand()}, resolver{
                                                                                                    strand.context()} {}

    inline asio::io_context::strand &get_strand() noexcept { return strand; }

    void init_start() noexcept override {
        subscribe(&http_worker_t::on_request);
        rotor::actor_base_t::init_start();
    }

    void init_finish() noexcept override { init_request.reset(); }

    bool try_shutdown() {
        if (shutdown_request) {
            if (resolver_active) {
                resolver.cancel();
            } else if (sock) {
                sys::error_code ec;
                sock->cancel(ec);
            } else {
                r::actor_base_t::shutdown_start();
            }
            return true;
        }
        return false;
    }

    void shutdown_start() noexcept override { try_shutdown(); }

    void on_request(message::http_request_t &req) noexcept {
        assert(!orig_req);
        assert(!shutdown_request);
        orig_req.reset(&req);
        response.clear();

        conditional_start();
    }

    void conditional_start() noexcept {
        if (sock) {
            auto self = r::intrusive_ptr_t<http_worker_t>(this);
            asio::defer(strand, [self = self]() {
                self->conditional_start();
                self->get_supervisor().do_process();
            });
        }
        start_request();
    }

    void start_request() noexcept {
        auto &url = orig_req->payload.request_payload.url;
        auto fwd = ra::forwarder_t(*this, &http_worker_t::on_resolve, &http_worker_t::on_resolve_error);
        resolver.async_resolve(url.host, url.port, std::move(fwd));
        resolver_active = true;
    }

    void request_fail(const sys::error_code &ec) noexcept {
        reply_with_error(*orig_req, ec);
        orig_req.reset();
        sock.reset();
    }

    void on_resolve_error(const sys::error_code &ec) noexcept {
        resolver_active = false;
        request_fail(ec);
        try_shutdown();
    }

    void on_resolve(resolve_results_t results) noexcept {
        resolver_active = false;
        if (try_shutdown()) {
            return;
        }

        sock = std::make_unique<tcp::socket>(strand.context());
        auto fwd = ra::forwarder_t(*this, &http_worker_t::on_connect, &http_worker_t::on_tcp_error);
        asio::async_connect(*sock, results.begin(), results.end(), std::move(fwd));
    }

    void on_tcp_error(const sys::error_code &ec) noexcept {
        request_fail(ec);
        try_shutdown();
    }

    void on_connect(resolve_it_t) noexcept {
        auto &url = orig_req->payload.request_payload.url;
        if (try_shutdown()) {
            return;
        }

        request.method(http::verb::get);
        request.version(11);
        request.target(url.path);
        request.set(http::field::host, url.host);
        request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        auto fwd = ra::forwarder_t(*this, &http_worker_t::on_request_sent, &http_worker_t::on_tcp_error);
        http::async_write(*sock, request, std::move(fwd));
    }

    void on_request_sent(std::size_t /* bytes */) noexcept {
        if (try_shutdown())
            return;

        auto fwd = ra::forwarder_t(*this, &http_worker_t::on_request_read, &http_worker_t::on_tcp_error);

        auto &rx_buff = orig_req->payload.request_payload.rx_buff;
        rx_buff->prepare(orig_req->payload.request_payload.rx_buff_size);
        http::async_read(*sock, *rx_buff, response, std::move(fwd));
    }

    void on_request_read(std::size_t bytes) noexcept {
        reply_to(*orig_req, std::move(response), bytes);
        orig_req.reset();

        sock->cancel();
        sock->close();
        sock.reset();

        try_shutdown();
    }

    asio::io_context::strand &strand;
    tcp::resolver resolver;
    tcp_socket_ptr_t sock;
    request_ptr_t orig_req;
    http::request<http::empty_body> request;
    http::response<http::string_body> response;
    bool resolver_active = false;
};

struct http_manager_config_t : public ra::supervisor_config_asio_t {
    using strand_ptr_t = ra::supervisor_config_asio_t::strand_ptr_t;
    std::size_t worker_count;
    r::pt::time_duration init_timeout;

    http_manager_config_t(std::size_t worker_count_, const r::pt::time_duration &init_timeout_,
                          const r::pt::time_duration &shutdown_duration_, strand_ptr_t srtand_)
        : ra::supervisor_config_asio_t{shutdown_duration_, srtand_}, worker_count{worker_count_}, init_timeout{
                                                                                                      init_timeout_} {}
};

struct http_manager_t : public ra::supervisor_asio_t {
    using workers_set_t = std::unordered_set<r::address_ptr_t>;
    using request_id = r::supervisor_t::timer_id_t;
    using request_ptr_t = r::intrusive_ptr_t<message::http_request_t>;
    using req_mapping_t = std::unordered_map<request_id, request_ptr_t>;

    http_manager_t(supervisor_t *parent_, const http_manager_config_t &config_)
        : ra::supervisor_asio_t{parent_, config_} {
        worker_count = config_.worker_count;
        init_timeout = config_.init_timeout;
    }

    void init_start() noexcept override {
        for (std::size_t i = 0; i < worker_count; ++i) {
            auto addr = create_actor<http_worker_t>(init_timeout)->get_address();
            workers.emplace(std::move(addr));
        }
        subscribe(&http_manager_t::on_request);
        subscribe(&http_manager_t::on_response);
        ra::supervisor_asio_t::init_start();
    }

    void shutdown_finish() noexcept override {
        ra::supervisor_asio_t::shutdown_finish();
        std::cerr << "http_manager_t::shutdown_finish()\n";
    }

    void on_request(message::http_request_t &req) noexcept {
        auto it = workers.begin();
        if (it == workers.end()) {
            std::abort();
        }
        auto worker_addr = *it;
        workers.erase(it);
        auto &payload = req.payload.request_payload;
        auto request_id = request<payload::http_request_t>(worker_addr, payload).send(payload.timeout);
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
    r::pt::time_duration init_timeout;
    workers_set_t workers;
    req_mapping_t req_mapping;
};

struct client_t : r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    using timepoint_t = std::chrono::time_point<std::chrono::high_resolution_clock>;

    void init_start() noexcept override {
        subscribe(&client_t::on_status);
        subscribe(&client_t::on_response);
        poll_status();
    }

    void shutdown_start() noexcept override {
        std::cerr << "client_t::shutdown_start\n";
        r::actor_base_t::shutdown_start();
    }

    void shutdown_finish() noexcept override {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        r::actor_base_t::shutdown_finish();
        double rps = success_requests / diff.count();

        std::cerr << "client_t::shutdown_finish, stats: " << success_requests << "/" << total_requests
                  << " requests, in " << diff.count() << "s, rps = " << rps << "\n";
        std::cerr << "client_t::shutdown_finish\n";
        manager_addr.reset();
        supervisor.do_shutdown(); /* trigger all system to shutdown */
    }

    void poll_status() noexcept {
        if (poll_attempts < 3) {
            request<r::payload::state_request_t>(manager_addr, manager_addr).send(timeout);
            ++poll_attempts;
        } else {
            supervisor.do_shutdown();
        }
    }

    void on_status(r::message::state_response_t &msg) noexcept {
        if (msg.payload.ec) {
            return supervisor.do_shutdown();
        }
        if (msg.payload.res.state != r::state_t::OPERATIONAL) {
            poll_status();
        } else {
            r::actor_base_t::init_start();
        }
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

    void on_start(r::message::start_trigger_t &msg) noexcept override {
        r::actor_base_t::on_start(msg);
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
            ("workers_count", po::value<std::size_t>()->default_value(10), "concurrency (number of http workers)")
            ("timeout", po::value<std::size_t>()->default_value(1000), "generic timeout (in milliseconds)")
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

    auto req_timeout = r::pt::milliseconds{vm["timeout"].as<std::size_t>()};
    auto workers_count = vm["workers_count"].as<std::size_t>();
    std::cerr << "using " << workers_count << " workers for " << url->host << ":" << url->port << url->path
              << ", timeout: " << req_timeout << "\n";
    asio::io_context io_context;

    auto system_context = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_context)};
    auto strand = std::make_shared<asio::io_context::strand>(io_context);
    auto man_timeout = req_timeout + r::pt::milliseconds{workers_count * 2};
    ra::supervisor_config_asio_t conf{man_timeout, strand};
    auto sup = system_context->create_supervisor<ra::supervisor_asio_t>(conf);

    http_manager_config_t http_conf{workers_count, man_timeout, man_timeout, strand};
    auto man = sup->create_actor<http_manager_t>(man_timeout, http_conf);

    auto client = sup->create_actor<client_t>(man_timeout);
    client->manager_addr = man->get_address();
    client->timeout = req_timeout;
    client->concurrency = workers_count;
    client->url = *url;
    client->max_requests = vm["max_requests"].as<std::size_t>();

    sup->start();
    io_context.run();
    return 0;
}
