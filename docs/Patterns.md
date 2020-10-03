# Patterns

[sobjectizer]: https://github.com/Stiffstream/sobjectizer
[request-response]: https://en.wikipedia.org/wiki/Request%E2%80%93response
[get-actor-address]: https://en.wikipedia.org/wiki/Actor_model#Synthesizing_addresses_of_actors

Networking mindset hint: try to think of messages as if they where UDP-datagrams,
supervisors as different network IP-addresses (which might or might not belong to
the same host), and actors as an opened ports (or as endpoints, i.e. as
IP-address:port).

## Multiple actor identities

Every actor has it's "main" address; however it is possible for it to have multiple addresses.
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

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        ...
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
            wsdl_addr = create_address();
            action_addr = create_address();
            p.subscribe_actor(&client_t::on_wsdl, wsdl_addr);
            p.subscribe_actor(&client_t::on_action, action_add);
        });
    }
    void on_wsdl(http_message_t& msg) noexcept {
        ...
    }

    void on_action(http_message_t& msg) noexcept {
        ...
    }

    void on_a_start() noexcept override {
        auto timeout = r::pt::seconds(1);
        request_via<htt::request_t>(http_client, wsdl_addr, /* request params */ ).send(timeout);
        request_via<htt::request_t>(http_client, action_addr, /* request params */ ).send(timeout);
    }
}
~~~


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
arrives in a moment later it should be discarded). All the underhood mechanics
is performed by supervisor, and there is a need of generic request/response
matching, which can be done by introducing some synthetic message id per request.
Hence, the response can't be just original user-defined payload, it's needed
to be enriched a little bit to.

A little bit more of terminology: the regular messages, which are not request
nor response might have different names to emphasize their role: signals,
notifications, or triggers.

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

using request_t = r::request_traits_t<payload::my_request_t>::request::message_t;
using response_t = r::request_traits_t<payload::my_request_t>::response::message_t;

}
~~~

Third, on the client side, the `request` method should be used (or `request_via` if
the answer is expected on the non-default address) and a bit specific access to
the user defined payload should be used, i.e.

~~~{.cpp}
struct client_actor_t : public r::actor_base_t {
    r::address_ptr_t server_addr;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>(
            [](auto &p) { p.subscribe_actor(&client_actor::on_response); });
    }


    void on_start() noexcept override {
        r::actor_base_t::on_start();
        auto timeout = r::pt::milliseconds{10};
        request<payload::sample_req_t>(server_addr, /* fields-forwaded-for-request-payload */)
            .send(timeout);
    }

    void on_response(message::response_t& msg) noexcept override {
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

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>(
            [](auto &p) { p.subscribe_actor(&server_actor::on_request); });
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
(if timeout timer already triggered), or it might be delivered further to the client.
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

## Registry

There is a known [get-actor-address] problem: how one actor should know the
address of the other actor? Well known way is to carefully craft initialization
taking addresses of just created actors and pass them in constructor to the
other actors etc. The approach will work in the certain circumstances; however it
leads to boilerplate and fragile code, which "smells bad", as some initialization
is performed inside actors and some is outside; it also does not handle well
the case of dynamic (virtual) addresses.

The better solution is to have "the registry" actor, known to all other actors.
Each actor, which provides some services registers it's main or virtual address
in the registry via some application-known string names; upon the termination
it undoes the registration. Each "client-actor" asks for the predefined service
point by it's name in the actor initialization phase; once all addresses for
the needed services are found, the initialization can continue and the actor
then becomes "operational".

Since the registry does not perform any I/O and can be implemented in loop-agnostic
way, it was included in `rotor` since `v0.06`, however the most convenient usage
comes with `v0.09`:


~~~{.cpp}
struct server_actor : public rotor::actor_base_t {
    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        ...
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.register_name("server", get_address()); });
    }
    ...
};


struct client_actor : public rotor::actor_base_t {
    rotor::address_ptr_t server_addr;
    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        ...
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.discover_name("server", server_addr, true).link(); });
    }
};

...
auto sup = system_context->create_supervisor<...>()
           ...
           .create_registry()
           .finish();

~~~

Please note, to let the things work, the `registry` actor is created by
supervisor, so all other child actors ("server" and "client") know the mediator.

During initialization phase the client actor discovers server address into the
`server_addr` variable, and when done, it links to it. If something goes wrong
it will shutdown, otherwise it will become operational. Additional synchronization
patterns are used here.

## Synchronization patterns

### Delayed discovery

There is a race condition in the registry example: the server-actor might register
self in the registry a little bit later than a client-actor asks for server-actor
address. The registry will reply to the client-actor with error, which will cause
client-actor to shutdown down.

~~~{.cpp}
    p.discover_name("server", server_addr, true);
~~~

The `true` parameter here asks the registry not to fail, but reply to the client-actor
as soon as server actor will register self.

What will happen, if the server-actor will never register self in the registry? Than
in the client-actor discovery timeout will trigger and the client-actor will shutdown.

### Linking actors

When two actors performing an interaction via messages they need to stay **operational**,
but they are some kind autonomous entities and can shut self down at any time. How
to deal with that? The actor linking mechanism was invented: when a client-actor is
linked to server-actor, and server actor is going to shutdown then it will ask the
client-actor to unlink. That way the client actor will shutdown too, but it still
have time to perform emergency cleaning (e.g. flush caches). Then a supervisor might
restart client and server actors, according to its policy. The API looks like:


~~~{.cpp}
struct client_actor : public rotor::actor_base_t {
    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        ...
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.discover_name("server", server_addr, true).link(false); }); // (1)

        plugin.with_casted<r::plugin::link_client_plugin_t>([&](auto &p) {
            p.link(addr_s, false, callback);                                            // (2)

    }
};
~~~

The `(1)` is convenient api to link after discovery, which the most likely you should use as
it covers most use cases. The `(2)` is more low-level API, where it is possible to setup
callback (it is available in `(1)` too), and the target actor address have to be specified.

The special `bool` argument available in the both versions make postponed link confirmation,
i.e. only upon server-actor start. Usually, this is have to be `false`.

It should be noted, that it is possible to make a cycle of linked actors. While this will
work, it will never shutdown properly. So, it is better to avoid cycles.

### Synchronized start

By default actor is started as soon as possible, i.e. once it confirmed initialization
its supervisor sends it start trigger. However, this behavior is not always desirable,
and there is need of barrier-like action, i.e. to let all child actors on a supervisor
start simultaneously. The special option `synchronized_start` is response for that:

~~~{.cpp}
    auto sup = system_context.create_supervisor<rotor::ev::supervisor_ev_t>()
                   .synchronize_start()
                   .timeout(...)
                   .finish();
~~~



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
by some `supervisor`. The sent to an address message is processed by
the supervisor: if the actor-subscriber is *local* (i.e. created on the `supervisor`),
then the message is delivered immediately to it, otherwise the message is wrapped
and forwarded to the supervisor of the actor (i.e. to some *foreign* supervisor),
and then it is unwrapped and delivered to the actor.

## Observer (mirroring traffic)

Each `actor` has it's own `address`. Due to MPMC-feature above it is possible that
first actor will receive messages for processing, and some other actor ( *foreign*
actor) is able to subscribe to the same kind of messages and observe them (with some
latency). It is possible observe even `rotor` "internal" messages, however it is
discouraged since there are more reliable synchronization approaches.

Let's assume that server-actor via non-rotor I/O somehow generates data (e.g.
it measures temperature once per second). Where should it send it if the consumers
of the data are dynamic (e.g. connected from network)? The solution is to send
metrics to itself, while dynamical clients should discover and link to the
sensor actor, and then subscribe to metrics on the sensor actor address.

~~~{.cpp}
namespace r = rotor;


namespace payload {
    struct temperature_t { double value; };
};

namespace message {
    using temperature_notification_t = message_t<payload::temperature_t>;
}

struct sensor_actor_t: public r::actor_base_t {
    // registration "service::sensor" is omitted
    void on_new_temperature(double value) {
        // yep, send it to itself
        send<payload::temperature_t>(address, value);
    }
};

struct client_actor_t: public r::actor_base_t {
    r::address_ptr_t sensor_addr;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::registry_plugin_t>([&](auto &p) {
            p.discover_name("service::sensor", sensor_addr)
                .link()
                .callback([&](auto phase, auto &ec) mutable {
                    if (phase == r::plugin::registry_plugin_t::phase_t::linking && !ec) {
                        auto subscriber = get_plugin(r::plugin::starter_plugin_t::class_identity);
                        static_cast<r::plugin::starter_plugin_t*>(subscriber)->subscribe_actor(&client_actor_t::on_temperature);
                    }
                });
        });
    }

    void on_temperature(message::temperature_notification_t& msg) noexcept {
        ...
    }
};
~~~

That way it is possible to "spy" messages of the sensor actor. To avoid synchronization
issues the client should discover and link to the sensor actor.

The distinguish of *foreign and non-foreign* actors or MPMC pattern is completely
**architectural** and application specific, i.e. whether it is known apriori that
there are multiple subscribers (MPMC) or single subscriber and other subscribes
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

Of course, actors can dynamically subscribe/unsubscribe from address at runtime.

## Actor overload protection (workload balancing)

[sobjectizer] ships with build-in message box protection, i.e. when inbound
message queue hits certain threshold an predefined action can be performed:
an message can be silently dropped (the newest one), it can be transformed to
some other kind of message, or actor or application can be shut down etc.

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
are here. The missing blocks are: handshake and message serialization, for
which it is needed to have reflections in C++.

The final goal is: the `send<payload>(destination_address, args...)` should
send the message to some *local* `destination_address`, which is the representative
of some *remote* peer actor address, where the addresses will be NAT-ed and message
will be serialized and transferred over the wire to remote host, where it (request)
will be deserialized, processed and replied back and reverse procedure will
happen.

Whilst the actual network transmission cannot implemented in a event loop agnostic
way, *I think* the abovementioned protocol seems quite easy in a loop independent.
This is the area of further `rotor` research & development.
