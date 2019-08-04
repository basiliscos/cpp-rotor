# Introduction

[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio/ "Boost Asio"
[wx-widgets]: https://www.wxwidgets.org/ "wxWidgets"
[actor-model]: https://en.wikipedia.org/wiki/Actor_model "Actor Model"
[sobjectizer]: https://github.com/Stiffstream/sobjectizer

`rotor` is event loop friendly C++ actor micro framework.

That means, that rotor is probably useless without **event loop**, e.g.
[boost-asio] or [wx-widgets] event loop; as a **framework** rotor imposes
some usage patters of [actor-model], however their amount a bare minimum,
hence it is **micro** framework.

As actor model and event loops are asynchronous by their nature, the underlying
intuition is to uplift the low level *events* of an event loop into high level
messages between actors, making high-level messages abstracted from the event loop;
hence `rotor` should provide message passing facilities between actors
*independently* from the used event loop(s).

`rotor` can be used in the applications, where different loop engines are used
together and it is desirable to write some loop-agnostic logic still having
message passing interface. The library can be used as lightweight loop abstraction
with actor-like flavor.

`rotor` should be considered **experimental** project, i.e. no stability in
API is guaranteed.

`rotor` is licensed on *MIT* license.

`rotor` is influenced by [sobjectizer].

## Hello World (loop-less)

This example is very artificial, and probably will never be met in the real-life,
as the supervisor does not uses any event loop:

~~~{.cpp}
#include "rotor.hpp"
#include <iostream>

struct hello_actor: public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void on_start(rotor::message_t<rotor::payload::start_actor_t> &) noexcept override {
        std::cout << "hello world\n";
        supervisor.do_shutdown();   // optional
    }
};

struct dummy_supervisor: public rotor::supervisor_t {
    void start_shutdown_timer() noexcept override {}
    void cancel_shutdown_timer() noexcept override {}
    void start() noexcept override {}
    void shutdown() noexcept override {}
    void enqueue(rotor::message_ptr_t) noexcept override {}
};


int main() {
    rotor::system_context_t ctx{};
    auto sup = ctx.create_supervisor<dummy_supervisor>();
    sup->create_actor<hello_actor>();
    sup->do_process();
    return 0;
}
~~~

## Hello World (boost::asio loop)

~~~{.cpp}
#include "rotor/asio.hpp"

namespace asio = boost::asio;
namespace pt = boost::posix_time;

struct hello_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void on_start(rotor::message_t<rotor::payload::start_actor_t> &) noexcept override {
        std::cout << "hello world\n";
        supervisor.do_shutdown();   // optional
    }
};

int main() {
    asio::io_context io_context;
    auto system_context = rotor::asio::system_context_asio_t::ptr_t{new rotor::asio::system_context_asio_t(io_context)};
    rotor::asio::supervisor_config_t conf{pt::milliseconds{500}};
    auto sup = system_context->create_supervisor<rotor::asio::supervisor_asio_t>(conf);

    auto hello = sup->create_actor<hello_actor>();

    sup->start();
    io_context.run();
    return 0;
}
~~~

It is obvious that the actor code is the same in both cases, however the system environment
and supervisors are different. In the last example the important property of `rotor` is
shown : it is **not intrusive** to event loops, i.e. an event loop runs on by itself, not
introducing additional environment/thread; as the consequence, rotor actor can be seamlessly
integrated with loops.

The `supervisor.do_shutdown()` just sends message to supervisor to perform shutdown procedure.
Then, in the code `io_context.run()` loop terminates, as long as *there are no any pending
event*. `rotor` does not makes run loop endlessly.

## Ping-pong example

The following example demonstrates how to send messages between actors.

~~~{.cpp}
#include "rotor.hpp"
#include <iostream>

struct ping_t {};
struct pong_t {};

struct pinger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void on_initialize(rotor::message_t<rotor::payload::initialize_actor_t> &msg) noexcept override {
        rotor::actor_base_t::on_initialize(msg);
        subscribe(&pinger_t::on_pong);
    }

    void on_start(rotor::message_t<rotor::payload::start_actor_t> &msg) noexcept override {
        rotor::actor_base_t::on_start(msg);
        send<ping_t>(ponger_addr);
    }

    void on_pong(rotor::message_t<pong_t> &) noexcept {
        std::cout << "pong\n";
        supervisor.do_shutdown(); // optional
    }

    rotor::address_ptr_t ponger_addr;
};

struct ponger_t : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void on_initialize(rotor::message_t<rotor::payload::initialize_actor_t> &msg) noexcept override {
        rotor::actor_base_t::on_initialize(msg);
        subscribe(&ponger_t::on_ping);
    }

    void on_ping(rotor::message_t<ping_t> &) noexcept {
        std::cout << "ping\n";
        send<pong_t>(pinger_addr);
    }

  private:
    rotor::address_ptr_t pinger_addr;
};

struct dummy_supervisor : public rotor::supervisor_t {
    void start_shutdown_timer() noexcept override {}
    void cancel_shutdown_timer() noexcept override {}
    void start() noexcept override {}
    void shutdown() noexcept override {}
    void enqueue(rotor::message_ptr_t) noexcept override {}
};

int main() {
    rotor::system_context_t ctx{};
    auto sup = ctx.create_supervisor<dummy_supervisor>();

    auto pinger = sup->create_actor<pinger_t>();
    auto ponger = sup->create_actor<ponger_t>();
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
#include <iostream>

namespace r = rotor;

struct payload_t {};

struct pub_t : public r::actor_base_t {

    using r::actor_base_t::actor_base_t;

    void set_pub_addr(const r::address_ptr_t &addr) { pub_addr = addr; }

    void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        r::actor_base_t::on_start(msg);
        send<payload_t>(pub_addr);
    }

    r::address_ptr_t pub_addr;
};

struct sub_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void set_pub_addr(const r::address_ptr_t &addr) { pub_addr = addr; }

    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&sub_t::on_payload, pub_addr);
    }

    void on_payload(r::message_t<payload_t> &) noexcept {
        std::cout << "received on " << static_cast<void *>(this) << "\n";
    }

    r::address_ptr_t pub_addr;
};

struct dummy_supervisor : public rotor::supervisor_t {
    void start_shutdown_timer() noexcept override {}
    void cancel_shutdown_timer() noexcept override {}
    void start() noexcept override {}
    void shutdown() noexcept override {}
    void enqueue(rotor::message_ptr_t) noexcept override {}
};

int main() {
    rotor::system_context_t ctx{};
    auto sup = ctx.create_supervisor<dummy_supervisor>();

    auto pub_addr = sup->create_address(); // (1)
    auto pub = sup->create_actor<pub_t>();
    auto sub1 = sup->create_actor<sub_t>();
    auto sub2 = sup->create_actor<sub_t>();

    pub->set_pub_addr(pub_addr);
    sub1->set_pub_addr(pub_addr);
    sub2->set_pub_addr(pub_addr);

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

