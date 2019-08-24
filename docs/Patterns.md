# Patterns

[sobjectizer]: https://github.com/Stiffstream/sobjectizer

Networking mindset hint: try to think of messages as if they where UDP-datagrams,
supervisors as different network IP-addresses (which might or might not belong to
the same host), and actors as an opened ports (or as endpoints, i.e. as
IP-address:port).

## Multiple Producers Multiple Consumers (MPMC aka pub-sub)

A `message` is delivered to `address`, independently of subscriber or subscribers,
i.e. to one `address` there can subscribed many actors, as well as messages
can be send from multiple sources to the same `address`.

It should be noted, that an **message delivery order is source-actor sequenced**,
so it is wrong assumption that the same message will be delivered simultaneously
to different subscribers (actors), if they belong to different supervisors/threads.
Never assume that, nor assume that the message will be delivered with some guaranteed
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

It should noted, that subscription request is regular `rotor` message, i.e. sequence
of arrival of messages is undefined as soon as they are generated in different places;
hence, an observer might be subscired *too late*, while the original
messages has already been delivered to original recipient and the observer "misses" the
message. See the pattern below how to synronize actors.

The distinguish of *foreign and non-foreign* actors or MPMC pattern is completely
**architectural** and application specific, i.e. whether is is known apriori that
there are multiple subscribers (MPMC) or single subsciber and other subscribes
are are hidden from the original message flow. There is no difference between them
at the `rotor` core, i.e.

~~~{.cpp}
    // MPMC: an address is shared between actors
    auto dest1 = supervisor->make_address();
    auto actor_a = sup->create_actor<...>();
    auto actor_b = sup->create_actor<...>();
    actor_a->set_destination(dest1);
    actor_b->set_destination(dest1);

    // observer: actor_c own address is exposed for actor_d
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

struct actor_A_t: public r::actor_base_t {

  void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
      r::actor_base_t::on_start(msg);
      subscribe(&actor_A_t::on_message);
  }

  void on_message(r::message_t<payload> &msg) noexcept {
    //processing logic is here
  }
};

struct actor_B_t : public r::actor_base_t {
  void set_target_addr(const r::address_ptr_t &addr) { target_addr = addr; }

  void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
      r::actor_base_t::on_start(msg);
      send<payload>(target_addr);
  }

  r::address_ptr_t target_addr;
};

int main() {
    ...;
    auto supervisor = ...;
    auto actor_a = supervisor->create_actor<actor_A_t>();
    auto actor_b = supervisor->create_actor<actor_B_t>();

    actor_b->set_target_addr(actor_b->get_address());
    supervisor->start();
    ...;
};
~~~

However here is a problem: the message delivery order is source-actor sequenced,
it migth happen `actor_b` started be before `actor_a`, and the message with payload
will be lost.

The following trick is possible:

~~~{.cpp}
struct actor_A_t: public r::actor_base_t {
    // instead of on_start
    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
      r::actor_base_t::on_initialize(msg);
      subscribe(&actor_A_t::on_message);
    }
}
~~~

or even that way:

~~~{.cpp}
struct actor_A_t: public r::actor_base_t {
    // instead of on_start / on_initialize
    void do_initialize(r::system_context_t* ctx) noexcept override {
        r::actor_base_t::do_initialize(ctx);
        subscribe(&actor_A_t::on_message);
    }
}
~~~

That tricky way will definitely work under certain circumstances, i.e. when
actors are created sequentially and they use the same supervisor etc.; however
in general case there will be unavoidable race, and the approach will not
work when different supervisors / event loops are used, or when some I/O
is involved in the scheme (i.e. it needed to establish connection before
subscription). This is not networking mindset neither.

The more robust approach is to start `actor_b` as usual, observe `on_start` event
from `on_initialize` and poll the `actor_a` status. Then, `actor_b` will either
first receive the `on_start` event from `actor_a`, which means that `actor_a` is ready,
or it will receive `r::message_t<r::payload::state_response_t>` and further analysis
should be checked (i.e. if status is `initialized` or `started` etc.).

Further, if it is desirable to scale this pattern, then `actor_b` should not even start
unless `actor_a` is started, then `actor_b` should suspend it's `on_initialize`
message. The following code demonstrates this approach:

~~~{.cpp}

struct actor_A_t: public r::actor_base_t {
    // we need to be ready to accept messages, when on_start message arrives
    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
      r::actor_base_t::on_initialize(msg);
      subscribe(&actor_A_t::on_message);
    }
}

struct actor_B_t : public r::actor_base_t {
    r::message_t<r::payload::initialize_actor_t> init_message;

    void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
        // we are not finished initialization:
        // r::actor_base_t::on_initialize(msg);
        init_message = msg;
        subscribe(&actor_B_t::on_a_state);
        subscribe(&actor_B_t::on_a_start, target_addr);
        poll_a_state();
    }

    void poll_a_state() noexcept {
        auto& sup_addr   = target_addr->supervisor.get_address();
        auto reply_addr  = get_address();
        // ask actor_a supervisor about actor_a state, and deliver reply back to me
        send<r::payload::state_request_t>(sup_addr, reply_addr, target_addr);
    }

    void finish_init() noexcept {
        r::actor_base_t::on_initialize(init_message);
        init_message.reset();
        unsubscribe(&actor_B_t::on_a_state);                // optional
        unsubscribe(&actor_B_t::on_a_start, target_addr);   // optional
    }

    void on_a_state(r::message_t<r::payload::state_response_t> &msg) noexcept {
        // initialization is not finished
        if (init_message) {
            auto& state = msg.payload.state;
            if (state == r::state_t::OPERATIONAL) {
                finish_init();
            } else {
                poll_a_state();
            }
        }
    }

    void on_a_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        if (init_message) {
            finish_init();
        }
    }
}
~~~

In real life, the things are a little bit more complex, however: `actor_a` might
*never* start, or it might not start withing the certain timeframe, or actor_a's
supervisor might not even reply. If nothing will be done, then `actor_b` will stuck
in inifinte polling, consuming CPU resources. Do handle the cases, in `poll_a_state` method
the timeout timer should be started and in `finish_init` it should be stopped; the
re-poll should be performed not immediately, but again after some timeout. Plus,
the `actor_b` should shutdown itself after certain number of attemps (or after some
timeout). Within `rotor` all timers are event loop specific, and timeouts are
application-specific, so, there is no generaral example, just an sketch of the idea.

## Actor overload protection (workload balancing)

[sobjectizer] ships with build-in message box protection, i.e. when inbound
message queue hits certain threshold an predefined action can be performed:
an message can be silently dropped (the newest one), it can be transformed to
some other kind of message, or actor or application can be shutted down etc.

In `rotor` there is no "inbound" queue, and the [sobjectizer]'s approach is
not flexible enough: the overloading not always measured in number of
unprocessed messages, it can be measured in time for processing single message.
For example, there is a queue of request to compute Nth-prime number. If the
N lies within 1000, then queue size of 1000 messages is probably OK; however
if there is an request to compute 10_000_000-th prime number an actor will
certainly be overloaded.

There can be at least two approaches, depending how fast the reaction to overload
should be triggered. In the simplest case, when there is no timeframe guarantee
for overload reaction, it can be do as the following: an custom `supervisor`
shoud be written, messages to protected supervisor should be delivered not
immediately, but with some delay (i.e. `loop->postone([&](supervisor->do_process())`)
and before message delivery to the actor the queue size (or other criteria for
overloading condition) should be checked, then overload-reaction should be performed.

Another approach will be write an front-actor, which will run on dedicated supervisor
/ thread. The actor will forward requests to protected worker-actor, if the
worker-actor answers within certain timeframe, or immediately react with overload
action. This will work, if the request-message, contains reply address, which
will be remembered and overwritten by front-actor, before forwaring the message
to worker-actor, and in the reply-message the address might be needed to be
overwritten too. The strategy can be extended to use several workers, and,
hence, provide application-specific load balancing.

## Real networking

This is not yet started, however a lot of building blocks for networking are
already here: the **location transparency**, message passing and reactiveness
are here. The missing blocks are: service discovery, handshake, and message
serialization.

The final goal is: the `send<payload>(destination_address, args...)` should
send the message to some *local* `destination_address`, which is the representative
of some *remote* peer actor address, where the addresses will be NAT-ed and message
will be serialized and transferred over the wire to remote host, where it (request)
will be deserialized, processed and replied back and reverse procedure will
happen.

Whilst the actual network transmission cannot implemented in a event loop agnostic
way, *I think* the abovementioned protocol seems quite an loop independent.
This is the area of further `rotor` research & development.
