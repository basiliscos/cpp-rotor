# Advanced examples

## ping-pong example

[rotor]: https://github.com/basiliscos/cpp-rotor
[request-response]: https://basiliscos.github.io/blog/2019/10/05/request-response-message-exchange-pattern/
[examples/boost-asio/ping-pong-timer.cpp]: https://github.com/basiliscos/cpp-rotor/blob/master/examples/boost-asio/ping-pong-timer.cpp
[examples/boost-asio/beast-scrapper.cpp]: https://github.com/basiliscos/cpp-rotor/blob/master/examples/boost-asio/beast-scrapper.cpp

The full source code can be seen at [examples/boost-asio/ping-pong-timer.cpp].

We would like to get some ping-pong system. There are enough examples of simple ping-pong messaging,
however here we'd like to **simulate unreliability of I/O** via the ping-pong and the toolset, available
in [rotor], to overcome the unreliability.

The sources of the unreliability are: a) the `pong` response message does not arrive in time;
b) error happens in I/O layer. The additional requirement will be c) to allow the entire system
to do clean shutdown at any time, e.g. on user press `CTRL+C` on terminal.

Let enumerate the **rules of the simulator**. To simulate the unreliability, let's assume that `ponger` actor will answer
not immediately upon `ping` request, but after some time and with some probability. "After some time" means
that sometimes it will respond in time, and sometimes too late. The "some probability" will simulate I/O
errors. As soon as `pinger` receives successful `pong` response it shuts down the entire simulator. However,
if the `pinger` actor does not receive any successful `pong` response during some time despite multiple
attempts, it should shut self down too. The rules are reified as the constants like:


```cpp
namespace constants {
static float failure_probability = 0.97f;
static pt::time_duration ping_timeout = pt::milliseconds{100};
static pt::time_duration ping_reply_base = pt::milliseconds{50};
static pt::time_duration check_interval = pt::milliseconds{3000};
static std::uint32_t ping_reply_scale = 70;
} // namespace constants
```

The `pinger` pings ponger during `check_interval` or shuts self down. The ponger generates response during
`50 + rand(70)` milliseconds with the `1 - failure_probability`.


Ok, let's go to the implementation. To make it reliable, we are going to use many patterns, but first of all,
let's use the [request-response] one:

~~~{.cpp}
namespace payload {
struct pong_t {};
struct ping_t {
    using response_t = pong_t;
};
} // namespace payload

namespace message {
using ping_t = rotor::request_traits_t<payload::ping_t>::request::message_t;
using pong_t = rotor::request_traits_t<payload::ping_t>::response::message_t;
using cancel_t = rotor::request_traits_t<payload::ping_t>::cancel::message_t;
} // namespace message
~~~

Since the  `v0.10` it is possible to cancel pending requests in [rotor]. Second pattern
will be the `discovery` (registry) pattern: here the `ponger_t` actor will act as a server
(i.e. it will announce self in a registry), and the `pinger_t` actor will act as a client,
(i.e. it will locate `ponger` in a registry by name):

~~~{.cpp}
struct pinger_t : public rotor::actor_base_t {
    ...
    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        ...
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.discover_name("ponger", ponger_addr, true).link(); });
    }


struct ponger_t : public rotor::actor_base_t {
    ...
    void configure(rotor::plugin::plugin_base_t &plugin) noexcept override {
        ...
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.register_name("ponger", get_address()); });
    }
~~~

The mediator (aka registry) actor have to be created; we'll instruct supervisor
to instantiate it for us:

~~~{.cpp}
int main() {

auto sup = system_context->create_supervisor<custom_supervisor_t>()
               ...
               .create_registry()
               ...
               .finish();
~~~

What should `pinger` do upon start? It should do `ping` ponger and spawn a timer to shut
self down upon timeout.

~~~{.cpp}
void on_start() noexcept override {
    rotor::actor_base_t::on_start();
    do_ping();

    timer_id = start_timer(constants::check_interval, *this, &pinger_t::on_custom_timeout);
    resources->acquire(resource::timer);
}

void on_custom_timeout(rotor::request_id_t, bool cancelled) {
    resources->release(resource::timer);
    timer_id.reset();
    std::cout << "pinger_t (" << (void *)this << "), on_custom_timeout, cancelled: " << cancelled << "\n";
    if (!cancelled) {
        do_shutdown();
    }
}
~~~

The `on_start` method is rather trivial, except the two nuances. First, it must record the `timer_id`,
which may be necessary for the timer cancellation on shutdown initiation. Second, it acquires the
timer *resource*, whose entire purpose is to delay shutdown (and, in general, the initialization) phase.
Without the resource acquisition, the timer might trigger *after* actor shutdown, which usually is
bad idea. In the timer handler (`on_custom_timeout`) it performs the reverse actions: the timer
*resource* is released (and, hence, shutdown will be continued if it was started), and `timer_id`
is reset to prevent cancellation in shutdown (will be shown soon). There is a guarantee, that
timer handler will be invoked once anyway, whether it was cancelled or triggered. However, in accordance
with our rules if it triggers, the `pinger` actor should shut self down (i.e. `do_shutdown()` method
is invoked).

Let's demonstrate the `pinger` shutdown procedure:

~~~{.cpp}
void shutdown_start() noexcept override {
    std::cout << "pinger_t, (" << (void *)this << ") shutdown_start() \n";
    if (request_id)
        send<message::cancel_t>(ponger_addr, get_address());
    if (timer_id) {
        cancel_timer(*timer_id);
        timer_id.reset();
    }
    rotor::actor_base_t::shutdown_start();
}
~~~

It's quite trivial: if there is a pending ping request, let's cancel it. If there is an active timer,
let's cancel it too. Otherwise, let's continue shutdown. It should be noted, that the acquired resources
are not released here; instead of the corresponding async operations are cancelled, and the resources
will be released upon cancellation. The [rotor] internals knows about resources, so, it is safe
to invoke `rotor::actor_base_t::shutdown_start()` here (and it should). Any further resource release
will continue suspended shutdown or initialization (see more about that in `rotor::plugin::resources_plugin_t`).


Let's see the ping request and reaction to ping response (`pong`) in `pinger`:

~~~{.cpp}
void do_ping() noexcept {
    resources->acquire(resource::ping);
    request_id = request<payload::ping_t>(ponger_addr).send(constants::ping_timeout);
    ++attempts;
}

void on_pong(message::pong_t &msg) noexcept {
    resources->release(resource::ping);
    request_id.reset();
    auto &ee = msg.payload.ee;
    if (!ee) {
        std::cout << "pinger_t, (" << (void *)this << ") success!, pong received, attempts : " << attempts << "\n";
        do_shutdown();
    } else {
        std::cout << "pinger_t, (" << (void *)this << ") pong failed (" << attempts << ")\n";
        if (timer_id) {
            do_ping();
        }
    }
}
~~~

Again, it follows the same pattern: initiate async operation (request), acquire resource, record it; and, upon response,
release the resource, forget the request. Upon shutdown (as it is shown above), cancel request if it exists. As for the
request processing flow, according to our rules, it shuts self down upon successful pong response; otherwise,
if the actor is still operational (i.e. `timer_id` does exist), it performs another ping attempt.

Let's move the `ponger` overview. As the actor plays the server role it usually does not have `on_start()` method.
As `ponger` does not reply immediately to ping requests, it should store them internally for further responses.

~~~{.cpp}
struct ponger_t : public rotor::actor_base_t {
    ...;
    using message_ptr_t = rotor::intrusive_ptr_t<message::ping_t>;
    using requests_map_t = std::unordered_map<rotor::request_id_t, message_ptr_t>;
    ...
    requests_map_t requests;
};
~~~

The key moment in the `requests_map_t` is `rotor::request_id_t`, which represents timer for each delayed ping response. So,
when ping request arrives, timer is spawned and stored with the smart pointer to the original request:


~~~{.cpp}
void on_ping(message::ping_t &req) noexcept {
    if (state != rotor::state_t::OPERATIONAL) {
        auto ec = rotor::make_error_code(rotor::error_code_t::cancelled);
        reply_with_error(req, ec);
        return;
    }
    ...
    auto timer_id = start_timer(reply_after, *this, &ponger_t::on_ping_timer);
    resources->acquire(resource::timer);
    requests.emplace(timer_id, message_ptr_t(&req));
}
~~~

As usual with async operations, the timer resource is acquired. However, there is additional check for the actor
state, as we don't want even to start async operation (timer), when actor is shutting down; in case actor
replies immediately with error.

The timer handler implementation isn't difficult:

~~~{.cpp}
void on_ping_timer(rotor::request_id_t timer_id, bool cancelled) noexcept {
    resources->release(resource::timer);
    auto it = requests.find(timer_id);

    if (!cancelled) {
        auto dice = dist(gen);
        if (dice > constants::failure_probability) {
            auto &msg = it->second;
            reply_to(*msg);
        }
    } else {
        auto ec = rotor::make_error_code(rotor::error_code_t::cancelled);
        reply_with_error(*it->second, ec);
    }
    requests.erase(it);
~~~

In other words, if the timer isn't cancelled, it *may be* replied with success, or, if it was cancelled
it replies with corresponding error code. Again, the timer resource is released, and request is erased
from the requests map. Actually, it can be implemented in a little bit more verbose way: respond with
error upon unsuccessful dice roll; however this is *not necessary*, due to the request-response
pattern it is protected by timer on the request side (i.e. in `pinger`).

Nonetheless it **does response with error** in the case of the cancellation, because the cancellation
usually happens during shutdown procedure, which it is desirable to finish ASAP, otherwise the shutdown
timer will trigger, and, by default, it will call `std::terminate`. That can be worked around via
tuning the shutdown timeouts (i.e. to let shutdown timeout be greater than the request timeout), however,
it is rather shaky ground and it is not recommended to follow.

The cancellation implementation is rather straightforward: it finds the timer/request pair by the
the original request id and origin (actor address), and then cancels the found timer. It should
be noted, that timer might have already been triggered and, hence, it is not found in the request map.

~~~{.cpp}
void on_cancel(message::cancel_t &notify) noexcept {
    auto request_id = notify.payload.id;
    auto &source = notify.payload.source;
    std::cout << "cancellation notify\n";
    auto predicate = [&](auto &it) {
        return it.second->payload.id == request_id && it.second->payload.origin == source;
    };
    auto it = std::find_if(requests.begin(), requests.end(), predicate);
    if (it != requests.end()) {
        cancel_timer(it->first);
    }
}
~~~

The `ponger` shutdown procedure is trivial: it just cancels all pending ping responses;
responses are deleted during cancellation callback invocation.

~~~{.cpp}
    void shutdown_start() noexcept override {
        while (!requests.empty()) {
            auto &timer_id = requests.begin()->first;
            cancel_timer(timer_id);
        }
        rotor::actor_base_t::shutdown_start();
    }
~~~

The final piece in the example is a custom supervisor:

~~~{.cpp}
struct custom_supervisor_t : ra::supervisor_asio_t {
    using ra::supervisor_asio_t::supervisor_asio_t;

    void on_child_shutdown(actor_base_t *, const std::error_code &) noexcept override {
        if (state < rotor::state_t::SHUTTING_DOWN) {
            do_shutdown();
        }
    }

    void shutdown_finish() noexcept override {
        ra::supervisor_asio_t::shutdown_finish();
        strand->context().stop();
    }
};
~~~

What is the need of it? First, because as for our rules, when `pinger` shuts down, the entire
system should shutdown too (Out of the box, in [rotor] a supervisor automatically shut self down only
if it's child has been shut down, while *the supervisor itself is in initialization stage*).
Second, it should stop boost::asio event loop and exit from `main()` function.

That it is. In my opinion it has moderate complexity, however the clean shutdown **scales well**,
if every actor has clean shutdown. And here is the demonstration of the thesis: you can
add many ping clients, and it still performs correctly the main logic as well as the clean
shutdown. That can be checked with tools like valgrind or memory/UB-sanitizers etc.

The output samples:

```
(all ping failed)
./examples/boost-asio/ping-pong-timer
pinger_t, (0x556d13bbd8a0) pong failed (1)
pinger_t, (0x556d13bbd8a0) pong failed (2)
pinger_t, (0x556d13bbd8a0) pong failed (3)
pinger_t, (0x556d13bbd8a0) pong failed (4)
pinger_t, (0x556d13bbd8a0) pong failed (5)
pinger_t, (0x556d13bbd8a0) pong failed (6)
pinger_t, (0x556d13bbd8a0) pong failed (7)
pinger_t, (0x556d13bbd8a0) pong failed (8)
pinger_t, (0x556d13bbd8a0) pong failed (9)
pinger_t, (0x556d13bbd8a0) pong failed (10)
pinger_t, (0x556d13bbd8a0) pong failed (11)
pinger_t, (0x556d13bbd8a0) pong failed (12)
pinger_t, (0x556d13bbd8a0) pong failed (13)
pinger_t, (0x556d13bbd8a0) pong failed (14)
pinger_t, (0x556d13bbd8a0) pong failed (15)
pinger_t, (0x556d13bbd8a0) pong failed (16)
pinger_t, (0x556d13bbd8a0) pong failed (17)
pinger_t, (0x556d13bbd8a0) pong failed (18)
pinger_t, (0x556d13bbd8a0) pong failed (19)
pinger_t, (0x556d13bbd8a0) pong failed (20)
pinger_t, (0x556d13bbd8a0) pong failed (21)
pinger_t, (0x556d13bbd8a0) pong failed (22)
pinger_t, (0x556d13bbd8a0) pong failed (23)
pinger_t, (0x556d13bbd8a0) pong failed (24)
pinger_t, (0x556d13bbd8a0) pong failed (25)
pinger_t, (0x556d13bbd8a0) pong failed (26)
pinger_t, (0x556d13bbd8a0) pong failed (27)
pinger_t, (0x556d13bbd8a0) pong failed (28)
pinger_t, (0x556d13bbd8a0) pong failed (29)
pinger_t, (0x556d13bbd8a0), on_custom_timeout, cancelled: 0
pinger_t, (0x556d13bbd8a0) shutdown_start()
pinger_t, (0x556d13bbd8a0) pong failed (30)
pinger_t, (0x556d13bbd8a0) finished attempts done 30
ponger_t, shutdown_finish

(11-th ping was successful)
./examples/boost-asio/ping-pong-timer
pinger_t, (0x55f9f90048a0) pong failed (1)
pinger_t, (0x55f9f90048a0) pong failed (2)
pinger_t, (0x55f9f90048a0) pong failed (3)
pinger_t, (0x55f9f90048a0) pong failed (4)
pinger_t, (0x55f9f90048a0) pong failed (5)
pinger_t, (0x55f9f90048a0) pong failed (6)
pinger_t, (0x55f9f90048a0) pong failed (7)
pinger_t, (0x55f9f90048a0) pong failed (8)
pinger_t, (0x55f9f90048a0) pong failed (9)
pinger_t, (0x55f9f90048a0) pong failed (10)
pinger_t, (0x55f9f90048a0) success!, pong received, attempts : 11
pinger_t, (0x55f9f90048a0) shutdown_start()
pinger_t, (0x55f9f90048a0), on_custom_timeout, cancelled: 1
pinger_t, (0x55f9f90048a0) finished attempts done 11
ponger_t, shutdown_finish

(premature termination via CTRL+C pressing)
./examples/boost-asio/ping-pong-timer
pinger_t, (0x55d5d95d98a0) pong failed (1)
pinger_t, (0x55d5d95d98a0) pong failed (2)
pinger_t, (0x55d5d95d98a0) pong failed (3)
pinger_t, (0x55d5d95d98a0) pong failed (4)
pinger_t, (0x55d5d95d98a0) pong failed (5)
pinger_t, (0x55d5d95d98a0) pong failed (6)
^Cpinger_t, (0x55d5d95d98a0) shutdown_start()
pinger_t, (0x55d5d95d98a0), on_custom_timeout, cancelled: 1
pinger_t, (0x55d5d95d98a0) pong failed (7)
pinger_t, (0x55d5d95d98a0) finished attempts done 7
ponger_t, shutdown_finish
```

The full source code can be seen at [examples/boost-asio/ping-pong-timer.cpp]. There is a more advanced
[examples/boost-asio/beast-scrapper.cpp] example too, however without detailed explanations.

## Supervising and spawning actors

[examples/thread/ping-pong-spawner.cpp]: https://github.com/basiliscos/cpp-rotor/blob/master/examples/thread/ping-pong-spawner.cpp

*Supervising* actors is simply define some reaction upon managed child actor termination.
Some of reactions are already build in [rotor](https://github.com/basiliscos/cpp-rotor): the default
`supervisor_policy_t::shutdown_self` policy of a supervisor, will shut down the
supervisor if (a) it is in `initializing` state, and (b) one of its child actors shuts
self down. This is a simple form of *failure escalation*: failure to initialize actor
causes failure to initialize its supervisor.

However, if a supervisor has started, failure in child actor will cause no effect.
To change that the special method `escalate_failure(true)` should be called in actor
builder. Another possibility is to invoke `autoshutdown_supervisor(true)`, the difference
is the following: `autoshutdown_supervisor` **unconditionally** shuts supervisor
down, when a child is down, while `escalate_failure` analyzes shutdown reason code:
if the code is normal (i.e. no error or failure caused an actor shutdown, but it
shut self down because it successfully accomplished its job), then there is nothing
to escalate.

The common reaction to actor shutdown is to give an actor another chance, i.e.
restart it (spawn a new instance instead of the terminated one). Here an `spawner`
comes into play with its various policies applied: restart only on failure, restart only
on successful termination, always restart, never restart, maximum number of restart
attempts, whether the spawn failure should be escalated etc. To prevent the underlying
system from chocking up, caused by frequently actor restarts, the restart frequency
(i.e. minimum amount of time have to be passed before the next actor restart attempt)
is introduced.

Here is an example, which demonstrates the features, described above. Let's introduce
the rules for a ping-pong: pong might reply with failure on ping request; we'd like
to shutdown pinger then and spawn a new instance upto 15 times. If there is no
luck even after 15 attempts, the supervisor should fail and exit. Here is the
relevant code:

```cpp
auto sup = ctx.create_supervisor<rth::supervisor_thread_t>()
    .timeout(timeout)
    .create_registry()
    .finish();
sup->create_actor<ponger_t>().timeout(timeout).finish();
auto pinger_factory = [&](r::supervisor_t &sup, const r::address_ptr_t &spawner) -> r::actor_ptr_t {
    return sup.create_actor<pinger_t>()
     .timeout(timeout)
     .spawner_address(spawner)
     .finish();
};
sup->spawn(pinger_factory)
    .max_attempts(15)                               /* don't do that endlessly */
    .restart_period(timeout)
    .restart_policy(r::restart_policy_t::fail_only) /* case: respawn on single ping fail */
    .escalate_failure()                             /* case: when all pings fail */
    .spawn();
```

The successful branch in `pinger` can be like that:


```cpp
struct pinger_t : public rotor::actor_base_t {
    ...
    void on_pong(message::pong_t &reply) noexcept {
        auto &ee = reply.payload.ee;
        // fail branch, handled by spawner
        if (ee) {
            std::cout << "err: " << ee->message() << "\n";
            return do_shutdown(ee);
        }
        std::cout << "pong received\n";
        // successful branch: manually shutdown supervisor
        supervisor->do_shutdown();
    }
};
```

The sample output (successfully terminated, eventually):

```
ping (pinger #1 0x55d3b09ab130)
pong, dice = 0.000809254, passes threshold : no
err: ponger 0x55d3b09abe80 request timeout
ping (pinger #2 0x55d3b09af090)
pong, dice = 0.446941, passes threshold : no
err: ponger 0x55d3b09abe80 request timeout
ping (pinger #3 0x55d3b09ae670)
pong, dice = 0.809191, passes threshold : no
err: ponger 0x55d3b09abe80 request timeout
ping (pinger #4 0x55d3b09adfd0)
pong, dice = 0.955792, passes threshold : yes
pong received
shutdown reason: supervisor 0x55d3b09a4350 normal shutdown
```

The whole code is available at [examples/thread/ping-pong-spawner.cpp].

## need more?

There is my another open-source project [syncspirit](https://github.com/basiliscos/syncspirit),
which uses `rotor` under hood. I recommend to look at it, if the shipped examples are too-trivial, and
don't give you an architectural insight of using `rotor`.
