# Introduction

[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio/ "Boost Asio"
[wx-widgets]: https://www.wxwidgets.org/ "wxWidgets"
[actor-model]: https://en.wikipedia.org/wiki/Actor_model "Actor Model"
[sobjectizer]: https://github.com/Stiffstream/sobjectizer
[rotor]: https://github.com/basiliscos/cpp-rotor

[rotor] is event loop friendly C++ actor micro framework.

That means, that rotor is probably useless without **event loop**, e.g.
[boost-asio] or [wx-widgets] event loop; as a **framework** rotor imposes
some usage patters of [actor-model], however their amount a bare minimum,
hence it is **micro** framework.

As actor model and event loops are asynchronous by their nature, the underlying
intuition is to uplift the low level *events* of an event loop into high level
messages between actors, making high-level messages abstracted from the event loop;
hence [rotor] should provide message passing facilities between actors
*independently* from the used event loop(s).

[rotor] can be used in the applications, where different loop engines are used
together and it is desirable to write some loop-agnostic logic still having
message passing interface. The library can be used as lightweight loop abstraction
with actor-like flavor.

[rotor] should be considered **experimental** project, i.e. no stability in
API is guaranteed.

[rotor] is licensed on *MIT* license.

[rotor] is influenced by [sobjectizer].

## Hello World (loop-less)

This example is very artificial, and probably will never be met in the real-life,
as the supervisor does not uses any event loop:

~~~{.cpp}
#include "rotor.hpp"
#include "dummy_supervisor.h"
#include <iostream>

struct hello_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        std::cout << "hello world\n";
        supervisor->do_shutdown();
    }
};

int main() {
    rotor::system_context_t ctx{};
    auto timeout = boost::posix_time::milliseconds{500}; /* does not matter */
    auto sup = ctx.create_supervisor<dummy_supervisor_t>().timeout(timeout).finish();
    sup->create_actor<hello_actor>().timeout(timeout).finish();
    sup->do_process();
    return 0;
}
~~~

It prints "hello world" and exits. The example uses `dummy_supervisor_t` (sources omitted),
which does almost nothing, but gives you idea what supervisor should do. The code with "real"
supervisor is shown below, however for pure messaging the kind of supervisor does not matter.

## Hello World (boost::asio loop)

~~~{.cpp}
namespace asio = boost::asio;
namespace pt = boost::posix_time;

struct server_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        std::cout << "hello world\n";
        do_shutdown();
    }
};

int main() {
    asio::io_context io_context;
    auto system_context = rotor::asio::system_context_asio_t::ptr_t{new rotor::asio::system_context_asio_t(io_context)};
    auto strand = std::make_shared<asio::io_context::strand>(io_context);
    auto timeout = boost::posix_time::milliseconds{500};
    auto sup = system_context->create_supervisor<rotor::asio::supervisor_asio_t>()
        .strand(strand)
        .timeout(timeout)
        .finish();

    sup->create_actor<server_actor>()
        .timeout(timeout)
        .autoshutdown_supervisor()
        .finish();

    sup->start();
    io_context.run();

    return 0;
}
~~~

It is obvious that the actor code is the almost the same in both cases, however the system environment
and supervisors are different. In the last example the important property of [rotor] is
shown : it is **not intrusive** to event loops, i.e. an event loop runs on by itself, not
introducing additional environment/thread; as the consequence, rotor actor can be seamlessly
integrated with loops.

The `supervisor.do_shutdown()` from previous example just sends message to supervisor to
perform shutdown procedure. However, it might be better to have `.autoshutdown_supervisor()` during
actor setup, as it will shutdown supervisor in any case (i.e. not only from `on_start()`).
Then, in the code `io_context.run()` loop terminates, as long as *there are no any pending
event*. [rotor] does not make loop run endlessly.

The `timeout` variable is used to spawn timers for actor initialization and shutdown requests.
As the actor does not do any I/O the operations will be executed immediately, and timeout
values do not matter.

## Ping-pong example

The following example demonstrates how to send messages between actors.

~~~{.cpp}
struct ping_t {};
struct pong_t {};

struct pinger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        send<ping_t>(ponger_addr);
    }

    void on_pong(rotor::message_t<pong_t> &) noexcept {
        std::cout << "pong\n";
        do_shutdown();
    }

    rotor::address_ptr_t ponger_addr;
};

struct ponger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
    }

    void on_ping(rotor::message_t<ping_t> &) noexcept {
        std::cout << "ping\n";
        send<pong_t>(pinger_addr);
    }

  private:
    rotor::address_ptr_t pinger_addr;
};

int main() {
    rotor::system_context_t ctx{};
    auto timeout = boost::posix_time::milliseconds{500}; /* does not matter */
    auto sup = ctx.create_supervisor<dummy_supervisor_t>().timeout(timeout).finish();

    auto pinger = sup->create_actor<pinger_t>()
                      .init_timeout(timeout)
                      .shutdown_timeout(timeout)
                      .autoshutdown_supervisor()
                      .finish();
    auto ponger = sup->create_actor<ponger_t>()
                      .timeout(timeout) // shortcut for init/shutdown
                      .finish();
    pinger->set_ponger_addr(ponger->get_address());
    ponger->set_pinger_addr(pinger->get_address());

    sup->do_process();
    return 0;
}
~~~

Without loss of generality the system context/supervisor can be switched to
any other and the example will still work, because the actors in the sample
are **loop-agnostic**, as well as the messages.

In the real world scenarios, actors might be not so pure, i.e. they
will interact with timers and other system events, however the interface for
sending messages is still the same (i.e. loop-agnostic), which makes it
quite a convenient way to send messages to actors running on *different loops*.


## pub-sub example

Any message can be send to any address; any actor, including actors running on
different supervisors and loops, can subscribe to any kind of message on any address.

~~~{.cpp}
#include "rotor.hpp"
#include "dummy_supervisor.h"
#include <iostream>

namespace r = rotor;

struct payload_t {};
using sample_message_t = r::message_t<payload_t>;

struct pub_t : public r::actor_base_t {

    using r::actor_base_t::actor_base_t;

    void set_pub_addr(const r::address_ptr_t &addr) { pub_addr = addr; }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        send<payload_t>(pub_addr);
    }

    r::address_ptr_t pub_addr;
};

struct sub_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void set_pub_addr(const r::address_ptr_t &addr) { pub_addr = addr; }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>(
            [&](auto &p) { p.subscribe_actor(&sub_t::on_payload, pub_addr); });
    }

    void on_payload(sample_message_t &) noexcept { std::cout << "received on " << static_cast<void *>(this) << "\n"; }

    r::address_ptr_t pub_addr;
};

int main() {
    rotor::system_context_t ctx{};
    auto timeout = boost::posix_time::milliseconds{500}; /* does not matter */
    auto sup = ctx.create_supervisor<dummy_supervisor_t>().timeout(timeout).finish();

    auto pub_addr = sup->create_address(); // (1)
    auto pub = sup->create_actor<pub_t>().timeout(timeout).finish();
    auto sub1 = sup->create_actor<sub_t>().timeout(timeout).finish();
    auto sub2 = sup->create_actor<sub_t>().timeout(timeout).finish();

    pub->set_pub_addr(pub_addr);
    sub1->set_pub_addr(pub_addr);
    sub2->set_pub_addr(pub_addr);

    sup->do_process();

    sup->do_shutdown();
    sup->do_process();

    return 0;
}
~~~

Output sample:
~~~
received on 0x55d159ea36c0
received on 0x55d159ea3dc0
~~~

The address in the line `(1)` is arbitrary: the address of pub-actor itself
as well as the address of the supervisor can be used... even address of different
supervisor can be used.

Please note, that since `v0.09` there is a new way of subscribing an actor to
messages: now it is done via `starter_plugin_t` and overriding `configure`
method of the actor.

## request-response example (boost::asio)

In the example below the usage of request-response pattern is demonstrated.
The "server" actor takes the number from request and replies to  "client" actor
with square root if the value is >= 0, otherwise it replies with error.

Contrary to the regular messages, request-response is a little bit more
complex pattern: the response should include full request (message) to be able
to match to the request, as well as the possibility of response not arrival
in time; in the last case the response message should arrive with error.

That's why below not pure user-defined payloads are used, but a little
bit modified to support request/reply machinery.


~~~{.cpp}
#include "rotor.hpp"
#include "rotor/asio.hpp"
#include <iostream>
#include <cmath>
#include <system_error>

namespace asio = boost::asio;
namespace pt = boost::posix_time;

namespace payload {
struct sample_res_t {
    double value;
};
struct sample_req_t {
    using response_t = sample_res_t;
    double value;
};
} // namespace payload

namespace message {
using request_t = rotor::request_traits_t<payload::sample_req_t>::request::message_t;
using response_t = rotor::request_traits_t<payload::sample_req_t>::response::message_t;
} // namespace message

struct server_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>(
            [](auto &p) { p.subscribe_actor(&server_actor::on_request); });
    }

    void on_request(message::request_t &req) noexcept {
        auto in = req.payload.request_payload.value;
        if (in >= 0) {
            auto value = std::sqrt(in);
            reply_to(req, value);
        } else {
            // IRL, it should be your custom error codes
            auto ec = std::make_error_code(std::errc::invalid_argument);
            reply_with_error(req, make_error(ec));
        }
    }
};

struct client_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    rotor::address_ptr_t server_addr;

    void set_server(const rotor::address_ptr_t addr) { server_addr = addr; }

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>(
            [](auto &p) { p.subscribe_actor(&client_actor::on_response); });
    }

    void on_response(message::response_t &res) noexcept {
        if (!res.payload.ee) { // check for possible error
            auto &in = res.payload.req->payload.request_payload.value;
            auto &out = res.payload.res.value;
            std::cout << " in = " << in << ", out = " << out << "\n";
        }
        do_shutdown();
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        auto timeout = rotor::pt::milliseconds{1};
        request<payload::sample_req_t>(server_addr, 25.0).send(timeout);
    }
};

int main() {
    asio::io_context io_context;
    auto system_context = rotor::asio::system_context_asio_t::ptr_t{new rotor::asio::system_context_asio_t(io_context)};
    auto strand = std::make_shared<asio::io_context::strand>(io_context);
    auto timeout = boost::posix_time::milliseconds{500};
    auto sup =
        system_context->create_supervisor<rotor::asio::supervisor_asio_t>().strand(strand).timeout(timeout).finish();
    auto server = sup->create_actor<server_actor>().timeout(timeout).finish();
    auto client = sup->create_actor<client_actor>().timeout(timeout).autoshutdown_supervisor().finish();
    client->set_server(server->get_address());
    sup->do_process();
    return 0;
}

~~~

Output sample:
~~~
in = 25, out = 5
~~~

## registry example (boost::asio)

There is a minor drawback in the last example: the binding of client and server actors is performed
manually, the server address is injected directly into client actor and there is no any guarantees
that server actor will "outlive" client actor.

As usual in the computer sciences all problems can be solved with additional layer of indirection.
We'll create special registry actor. When server actor will be ready, it will register self there
under symbolic name "server"; the client actor will look up in the registry server address by
the same symbolic name and "link" to the server actor, i.e. make sure that server actor will
have longer lifetime than client actor.


~~~{.cpp}
#include "rotor.hpp"
#include "rotor/asio.hpp"
#include <iostream>
#include <cmath>
#include <system_error>
#include <random>

namespace asio = boost::asio;
namespace pt = boost::posix_time;

namespace payload {
struct sample_res_t {
    double value;
};
struct sample_req_t {
    using response_t = sample_res_t;
    double value;
};
} // namespace payload

namespace message {
using request_t = rotor::request_traits_t<payload::sample_req_t>::request::message_t;
using response_t = rotor::request_traits_t<payload::sample_req_t>::response::message_t;
} // namespace message

struct server_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>(
            [](auto &p) { p.subscribe_actor(&server_actor::on_request); });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.register_name("server", get_address()); });
    }

    void on_request(message::request_t &req) noexcept {
        auto in = req.payload.request_payload.value;
        if (in >= 0) {
            auto value = std::sqrt(in);
            reply_to(req, value);
        } else {
            // IRL, it should be your custom error codes
            auto ec = std::make_error_code(std::errc::invalid_argument);
            reply_with_error(req, make_error(ec));
        }
    }
};

struct client_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    rotor::address_ptr_t server_addr;

    void set_server(const rotor::address_ptr_t addr) { server_addr = addr; }

    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        rotor::actor_base_t::configure(plugin);
        plugin.with_casted<rotor::plugin::starter_plugin_t>(
            [](auto &p) { p.subscribe_actor(&client_actor::on_response); });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.discover_name("server", server_addr, true).link(); });
    }

    void on_response(message::response_t &res) noexcept {
        if (!res.payload.ee) { // check for possible error
            auto &in = res.payload.req->payload.request_payload.value;
            auto &out = res.payload.res.value;
            std::cout << " in = " << in << ", out = " << out << "\n";
        }
        do_shutdown(); // optional;
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        auto timeout = rotor::pt::milliseconds{1};
        request<payload::sample_req_t>(server_addr, 25.0).send(timeout);
    }
};

int main() {
    asio::io_context io_context;
    auto system_context = rotor::asio::system_context_asio_t::ptr_t{new rotor::asio::system_context_asio_t(io_context)};
    auto strand = std::make_shared<asio::io_context::strand>(io_context);
    auto timeout = boost::posix_time::milliseconds{500};
    auto sup = system_context->create_supervisor<rotor::asio::supervisor_asio_t>()
                   .strand(strand)
                   .create_registry()
                   .timeout(timeout)
                   .finish();
    auto server = sup->create_actor<server_actor>().timeout(timeout).finish();
    auto client = sup->create_actor<client_actor>().timeout(timeout).autoshutdown_supervisor().finish();
    sup->do_process();
    return 0;
}
~~~

## there is more...

There is more rotor capabilities, like requests cancellations, and generalized timer
spawning interface. Read on in the [Advanced Examples](Examples.md) section.
