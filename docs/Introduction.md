# Introduction

[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio/ "Boost Asio"
[wx-widgets]: https://www.wxwidgets.org/ "wxWidgets"
[actor-model]: https://en.wikipedia.org/wiki/Actor_model "Actor Model"
[sobjectizer]: https://github.com/Stiffstream/sobjectizer

Rotor is event loop friendly C++ actor micro framework.

That means, that rotor is probably useless without **event loop**, e.g.
[boost-asio] or [wx-widgets] event loop; as a **framework** rotor imposes
some usage patters of [actor-model], however their amount a bare minimum,
hence it is **micro** framework.

As actor model and event loops are asynchronous by their nature, the underlying
intuion is to uplift the low level *events* of an event loop into hight-level
messages between actors, making high-level messages abstrated from the event loop;
hence `rotor` should provide message passing facilities between actors
*independenly* from the used event loop(s).

Rotor should be considered **experimental** project, i.e. no stability in
API is guaranteed.

Rotor is licenced on *MIT* license.

Rotor is influenced by [sobjectizer].

## Hello World (loop-less)

This example is very artificial, and probably will never be met in the real-life,
as the supervisor does not uses any event loop

~~~~~~~~~~~~~~{.cpp}
#include "rotor/asio.hpp"
#include <iostream>

struct hello_actor: public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void on_start(rotor::message_t<rotor::payload::start_actor_t> &) noexcept override {
        std::cout << "hello world\n";
        do_shutdown();
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
~~~~~~~~~~~~~~

