# Patterns

[sobjectizer]: https://github.com/Stiffstream/sobjectizer
[request-response]: https://en.wikipedia.org/wiki/Request%E2%80%93response
[get-actor-address]: https://en.wikipedia.org/wiki/Actor_model#Synthesizing_addresses_of_actors

Networking mindset hint: try to think of messages as if they where UDP-datagrams,
supervisors as different network IP-addresses (which might or might not belong to
the same host), and actors as an opened ports (or as endpoints, i.e. as
IP-address:port).

## Request/Responce

While [request-response] approach is widely know, it has it's own specific
on the actor-model:

-# the response (message) arrives asynchronously and there is need to match
the original request (message)
-# the response might not arrive at all (e.g. an actor is down)

The first issue is solved in rotor via including full original message
(intrusive pointer) into response (message). This also means, that the receiver
(the "server") replies not to the message with the original user-defined payload, but
slightly enreached one; the same relates to the response (the "client" side).

The second issues is solved via spawning a *timer*. Obviously, that the timer
should be spawned on the client-side. In the case of timeout, the client-side
should receive the response message with the timeout error (and if the response
arrives in a moment later it should be discarded). All the underhood meachanics
is performed by supervisor, and there is a need of generic request/response
matching, which can be done by introducing some synthetic message id per request.
Hence, the request can't be just original user-defined payload, it's needed
to be enriched a little bit to.

`rotor` provides support for the [request-response] pattern.

First, you need to define your payloads in the request and response messages,
linking the both types

~~~{.cpp}
namespace payload {

struct my_response_t {
    // my data fields
};

struct my_request_t {
    using response_t = my_response_t;
    // my data fields
}

};
~~~

Second, you need to wrap them to let `rotor` knows that this is request/response pair:

~~~{.cpp}
namespace message {

using request_t = rotor::request_traits_t<payload::my_request_t>::request::message_t;
using response_t = rotor::request_traits_t<payload::my_request_tt>::response::message_t;

}
~~~

Third, on the client side, the `request` method should be used (or `request_via` if
the answer is expected on the non-default address) and a bit specific access to
the user defined payload should be used, i.e.

~~~{.cpp}
struct client_actor_t : public r::actor_base_t {
    r::address_ptr_t server_addr;

    void init_start() noexcept override {
        subscribe(&client_actor_t::on_reply);
        r::actor_base_t::init_start();
    }

    void on_start(r::message::start_trigger_t &msg) noexcept override {
        auto timeout = r::pt::milliseconds{10};
        request<payload::my_request_t>(server_addr /*, fields-forwaded-for-request-payload */)
            .send(timeout);
    }

    void on_reply(message::response_t& msg) noexcept override {
        if (msg.payload.ec) {
            // react somehow to the error, i.e. timeout
            return;
        }
        auto& req = msg.payload.req->payload; // original request payload
        auto& res = msg.payload.res;          // original response payload
    }
}
~~~

Forth, on the server side the `reply_to` or `reply_with_error` methods should be used, i.e.:

~~~{.cpp}
struct server_actor_t : public r::actor_base_t {
    r::address_ptr_t server_addr;

    void init_start() noexcept override {
        subscribe(&server_actor_t::on_request);
        r::actor_base_t::init_start();
    }


    void on_request(message::request_t& msg) noexcept override {
        auto& req = msg.payload.request_payload; // original request payload
        if (some_condition) {
            reply_to(msg, /*, fields-forwaded-for-response-payload */);
            return;
        }
        std::eror_code ec = /* .. make somehow app-specific error code */;
        reply_with_error(msg, ec);
    }
}
~~~

However, the story does not end here. As you might already guess, the response
message arrives to the client supervisor first, where it might be discarded
(if timeout timer already triggered), or it migth be delivered further to the client.
As the `rotor` library should not modify the user-defined message at will,
the new response message is created *via copying* the original one. As this
might be not desirable, rotor is able to handle that: instead of copying
the content, the intrusive pointer to it can be created, i.e.

~~~{.cpp}
namespace payload {

struct my_response_t:  r::arc_base_t<my_response_t>  {  // intrusive pointer support
    // my data fields
    explicit my_response_t(int value_) { ... } // the constructor must be provided
    virtual ~my_response_t() {}                // the virtual destructor must be provided
};

struct my_request_t {
    using response_t = r::intrusive_ptr_t<my_response_t>;   // that's also changed
    // my data fields
}

};
~~~

That's way responses, with heavy to- copy payload might be created.
See `examples/boost-asio/request-response.cpp` as the example.

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

    void init_start() noexcept override {
        subscribe(&observer_t::on_target_initialize, observable);
        subscribe(&observer_t::on_target_start, observable);
        subscribe(&observer_t::on_target_shutdown, observable);
        r::actor_base_t::init_start();
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
**architectural** and application specific, i.e. whether it is known apriori that
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

## Multiple actor identities

Every actor has it's "main" address; however it is possible for it to have multiple.
This makes it available to have "inside actor" routing, or polymorphism. This
is useful when the **same type** of messages arrive in response to different queries.

For example, let's assume that there is an "http-actor", which is able to "execute"
http requests in generic way and return back the replies. If there is a SOAP/WSDL
-webservice, the first query will be "get list of serices", and the second query
will be "execute an action X". The both responses will be HTTP-replies.

Something like the following can be done:

~~~{.cpp}

struct client_t: public r::actor_base_t {
    r::address_ptr_t http_client;
    r::address_ptr_t wsdl_addr;
    r::address_ptr_t action_addr;

    void init_start() noexcept override {
        ...
        wsdl_addr = create_address();
        action_addr = create_address();
        subscribe(&client_t::on_wsdl, wsdl_addr);
        subscribe(&client_t::on_action, action_addr);

    }

    void on_wsdl(http_message_t& msg) noexcept {
        ...
        auto timeout = r::pt::seconds(1);
        request_via<htt::request_t>(http_client, action_addr, /* request params */ ).send(timeout);
    }

    void on_action(http_message_t& msg) noexcept {
        ...
    }

    void on_a_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        ...
        auto timeout = r::pt::seconds(1);
        request_via<htt::request_t>(http_client, wsdl_addr, /* request params */ ).send(timeout);
    }
}
~~~

## Registry

There is a known [get-actor-address] problem: how one actor should know the
address of the other actor? Well known way is to carefully craft initialization
taking addresses of just created actors and pass them in constructor to the
other actors etc. The approach will work in the certain circumstances; however it
leads to boilerplate and fragile code, which "smells bad", as some initialization
is performed inside actors and some is outside; it also does not handles well
the case of dynamic (virtual) addresses.

The better solution is to have "the registry" actor, known to all other actors.
Each actor, which provides some services registers it's main or virtual address
in the registry via some application-known string names; upon the termination
it undoes the registration. Each "client-actor" asks for the predefined service
point by it's name in the actor initialization phase; once all addresses for
the needed services are found, the initialization can continue and the actor
then becomes "operational".

Since the registry does not perform any I/O and can be implemented in loop-agnostic
way, it was included in `rotor` since `v0.06`.

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
    void init_start() noexcept override {
        subscribe(&actor_A_t::on_message);
        r::actor_base_t::init_start()
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
from `on_initialize` and poll (request) the `actor_a` status. Then, `actor_b` will either
first receive the `on_start` event from `actor_a`, which means that `actor_a` is ready,
or it will receive `r::message::state_response_t` and further analysis
should be checked (i.e. if status is `initialized` or `started` etc.).

Further, if it is desirable to scale this pattern, then `actor_b` should not even start
unless `actor_a` is started, then `actor_b` should suspend it's `init_start`
message. The following code demonstrates this approach:

~~~{.cpp}

struct actor_A_t: public r::actor_base_t {
    // we need to be ready to accept messages, when on_start message arrives
    void init_start() noexcept override {
        subscribe(&actor_A_t::on_message);
        r::actor_base_t::init_start()
    }
}

struct actor_B_t : public r::actor_base_t {
    r::message_t<r::payload::initialize_actor_t> init_message;

    void init_start() noexcept override {
        // we are not finished initialization:
        // r::actor_base_t::init_start();
        subscribe(&actor_B_t::on_a_state);
        subscribe(&actor_B_t::on_a_start, target_addr);
        poll_a_state();
    }

    void poll_a_state() noexcept {
        auto& sup_addr   = target_addr->supervisor.get_address();
        auto reply_addr  = get_address();
        // ask actor_a supervisor about actor_a state, and deliver reply back to me
        auto timeout = r::pt::seconds{1};
        request<r::payload::state_request_t>(sup_addr, target_addr).send(timeout);
    }

    void finish_init() noexcept {
        r::actor_base_t::init_start()
        unsubscribe(&actor_B_t::on_a_state);                // optional
        unsubscribe(&actor_B_t::on_a_start, target_addr);   // optional
    }

    void on_a_state(r::message::state_response_t &msg) noexcept {
        if (state == r::state_t::INITIALIZED) {
            return; // we are already initialized
        }
        if (msg.payload.ec) {
            return do_shutdown(); // something bad happen
        }
        auto target_state = msg.payload.res.state;
        if (state == r::state_t::OPERATIONAL) {
            finish_init();
        } else {
            poll_a_state();
        }
    }

    void on_a_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
        if (init_message) {
            finish_init();
        }
    }
}
~~~

We covered some cases, i.e. when there is no any actor is listening on the address, or
when the supervisor does not replies with the certain timeframe - in that cases
the `response_t` will contain error code with timeout. Howevere, a few things
still need to be done: it should be prevented to from infinite polling. To do that
it should use some finite couter; if all attemps failed, then initiate `actor_B`
shutdown.

The sample does not cover the case, when `actor_A` decided to shutdown,
the `actor_B` should be notified and take appropriate actions. As it seems
generic pattern (i.e. `actor_B` uses `actor_A` as a client), this pattern,
probably, will be supported in the future version of `rotor` core.

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
