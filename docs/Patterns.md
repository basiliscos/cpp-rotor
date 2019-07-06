# Patterns

Networking mindset hint: try to think of messages as if they where UDP-datagrams,
supervisors as different network IP-addresses (which might or might not belong to
the same host), and actors as an opened ports (or as endpoints, i.e. as IPv4
address + port).

## Multiple Producers Multiple Consumers (MPMC)

An `message` is delivered to `address`, independently subscriber or subscribers,
i.e. to one `address` there can subscribed many actors, as well as messages
can be send from multiple sources to the same `address`.

It should be noted, that an **message delivery order is undefined**, so it is wrong
assumption that the same message will be delivered simultaneously to different
subscribers (actors), if they belong to different supervisors/threads. Never
assume that, nor assume that the message will be delivered with some guaranteed
timeframe.

Technically in `rotor` it is implemented the following way: `address` is produced
by some `supervisor`. The sent to the addres message it is processed by
the supervisor: if the actor-subscriber is *local* (i.e. created on the `supervisor`),
then the message is delivered immediately to it, othewise the message is wrapped
and forwarded to the supervisor of the actor (i.e. to some *foreign* supervisor),
and then it is unwrapped and delivered to the actor.

## Observer (mirroring traffic)

Each `actor` has it's own `address`. Due to MPMC-feature above it is possible that
first actor will receive messages for processing, and some other actor ( *foreign*
actor) is able to subscribe to the same kind of messages and observe them (with some
latency). It is possible observe even `rotor` "internal" messages, which are
part of the API. In other words it is possible to do something like:

~~~{.cpp}
namespace r = rotor;

struct observer_t: public r::actor_base_t {
    r::address_ptr_t observable;
    void set_observable(r::address_ptr_t addr) { observable = std::move(addr); }

    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
        r::actor_base_t::on_initialize(msg);
        subscribe(&observer_t::on_target_initialize, observable);
        subscribe(&observer_t::on_target_start, observable);
        subscribe(&observer_t::on_target_shutdown, observable);
    }

    void on_target_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept {
        // ...
    }

    void on_target_start(r::message_t<r::payload::start_actor_t> &) noexcept {
        // ...
    }

    void on_target_shutdown(r::message_t<r::payload::shutdown_request_t> &) noexcept {
        // ...
    }
};

int main() {
    ...
    auto observer = sup->create_actor<observer_t>();
    auto target_actor = sup->create_actor<...>();
    observer->set_observable(sample_actor->get_address());
    ...
}
~~~

It should noted, that subscription request is regular `rotor` message, i.e. no any order
delivery guaranties; hence, an observer might be subscired *too late*, while the original
messages has already been delivered to original recipient and the observer "misses" the
message. See the pattern below how to synronize actors.

The distinguish of *foreign and non-foreign* actors or MPMC-address is completely
**architectural** and application specific, i.e. whether is is known apriory that
there are multiple subscribers (MPMC) or single subsciber and other subscribes
are are hidden from the original message flow. There is no difference between them
at the `rotor` core, i.e.

~~~{.cpp}
    // MPMC
    auto dest1 = supervisor->make_address();
    auto actor_a = sup->create_actor<...>();
    auto actor_b = sup->create_actor<...>();
    actor_a->set_destination(dest1);
    actor_b->set_destination(dest1);

    // observer
    auto actor_c = sup->create_actor<...>();
    auto actor_d = sup->create_actor<...>();
    actor_d->set_c_addr(actor_c->get_address());
~~~

Of course, actors can dynamically subscribe/unsubscribe from address at runtime

## check actor ready state (syncrhonizing stream)

Let's assume there are two actors, which need to communicate:

~~~{.cpp}

namespace r = rotor;

struct payload{};

struct actor_A_t : public r::actor_base_t {
  void set_target_addr(const r::address_ptr_t &addr) { target_addr = addr; }

  void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
      r::actor_base_t::on_start(msg);
      send<payload>(target_addr);
  }

  r::address_ptr_t target_addr;
};

struct actor_B_t: public r::actor_base_t {

  void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
      r::actor_base_t::on_start(msg);
      subscribe(&actor_B_t::on_message);
  }

  void on_message(r::message_t<payload> &msg) noexcept {
    //processing logic is here
  }
};

int main() {
    ...;
    auto supervisor = ...;
    auto actor_a = sup->create_actor<actor_A_t>();
    auto actor_b = sup->create_actor<actor_B_t>();

    actor_a->set_target_addr(actor_b->get_address());
    supervisor->start();
    ...;
};
~~~

However here is a problem: the message delivery order is not guaranteed, it migth
happen `actor_a` started be before `actor_b`, and the message with payload will be
lost.


## real networking

