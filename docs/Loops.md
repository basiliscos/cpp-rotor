# Event loops & platforms

## Event loops & backends

[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio/ "Boost Asio"
[wx-widgets]: https://www.wxwidgets.org/ "wxWidgets"
[ev]: http://software.schmorp.de/pkg/libev.html
[libevent]: https://libevent.org/
[std-thread]: https://en.cppreference.com/w/cpp/thread/thread
[libuv]: https://libuv.org/
[gtk]: https://www.gtk.org/
[qt]: https://www.qt.io/
[issues]: https://github.com/basiliscos/cpp-rotor/issues

 event loop   | support status
--------------|---------------
[boost-asio]  | supported
[wx-widgets]  | supported
[ev]          | supported
[std-thread]  | supported
[libevent]    | planned
[libuv]       | planned
[gtk]         | planned
[qt]          | planned

If you need some other event loop or speedup inclusion of a planned one, please file an [issue][issues].

## platforms

event loop   | support status
-------------|---------------
linux        | supported
windows      | supported
macos        | supported


## Notes on std::thread backend

This backend was developed to support **blocking** operations,  which have the same
effect as `std::this_thread::sleep_for` invocation. This is suitable for disk I/O
operations, working with database or using third-party libraries with synchronous
semantics.

It should be noted, that using blocking operations have some consequences, e.g.
actors on a thread, which executes blocking operation, will not be able to respond
to messages, namely, to cancellation requests. There is no universal remedy, however,
this can be smoothed: split long I/O operation into "continuation-message" with series
of smaller I/O chunks and when the current chunk is complete, just send self a message with
all work and index of the next chunk. It will make an actor more responsive, giving
it some breadth to pull external messages, trigger timeouts, and, finally, give it a
chance to react to cancellation notice.

When there are no messages, the backend uses `sleep_for` / `sleep_until` functions to
wait either external message message or until nearest timeout occurs. To let the things
work properly, the message handlers with blocking operations should specially marked
(`tag_io()`), to correctly update timers before, after and inside the handler.

See, `Blocking I/O multiplexing` in `Patterns`.

## Integration with event loops

`rotor` is designed to be integrated with event loops, which actually perform some I/O, spawn and
trigger timers and other asynchronous operations, which can be invoked from an actor or a supervisor.
The key accessor here is a supervisor.

First, the final (concrete) supervisor have to be obtained. Then, for safety, the intrusive pointer
to the actor should be created(2), and then loop-specific async operation should be initiated(3), where the pointer
should be moved(4). Finally, when the result of I/O operation is transformed into rotor message or messages(5),
which should be sent, the supervisor should be asked to perform rotor mechanics, i.e.
deliver message(s)(6). Let's illustrate it on timer using [boost-asio].

~~~{.cpp}
struct my_actor_t : r::actor_base_t {

    using timer_ptr_t = std::unique_ptr<boost::asio::deadline_timer>;
    timer_ptr_t timer;

    void on_trigger(message::some_message_t&) noexcept {
        using actor_ptr_t = r::intrusive_ptr_t<my_actor_t>;
        auto sup = static_cast<r::asio::supervisor_asio_t*>(supervisor); // (1)
        auto actor_ptr = actor_ptr_t(this); // (2)

        // (3)
        auto& strand = sup_ptr->get_strand();
        timer = std::make_unique<boost::asio::deadline_timer>(strand);
        timer->expires_from_now(timeout);
        timer->async_wait([this, actor_ptr = std::move(actor_ptr)](auto& ec){ // (4)
            ...;
            send<message::io_result_t>(...); // (5)
            get_supervisor()->do_process(); // (6)
            (void)actor_ptr;
        });
    }
};
~~~


## Adding loop support guide

Adding new event loop to `rotor` is rather simple: the new `supervisor` class
should be derived from `supervisor_t`, and the following methods should be
implemented:

~~~{.cpp}
void do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept override;
void do_cancel_timer(request_id_t timer_id) noexcept override;

void start() noexcept override;
void shutdown() noexcept override;

void enqueue(rotor::message_ptr_t) noexcept override;
~~~

The `enqueue` method is responsible for putting an `message` into `supervisor`
inbound queue *probably* in **thread-safe manner** to allow accept messages
from other supervisors/loops (thread-safety requirement) or just from some
outer context, when supervisor is still not running on the loop (can be
thread-unsafe). The second requirement is to let the supervisor process
all it's inbound messages *in a loop context*, may be with supervisor-specific
context.

The `start` and `shutdown` are just convenient methods to start processing
messages in event loop context (for `start`) and send a shutdown messages
and process event loop in event loop context .

`do_start_timer` should strate a new timer, whose id (request_id_t) can be
get via the `timer_handler_base_t`. The `do_cancel_timer` should cancel
timer and **immediately** invoke the timer_handler with `cancelled = true`.
The backend timer cancel implementation can be delayed, but that's actually
outsize of `rotor`.

Here is an skeleton example for `enqueue`:

~~~{.cpp}
void some_supervisor_t::enqueue(message_ptr_t message) noexcept {
    supervisor_ptr_t self{this};    // increase ref-counter
    auto& loop = ...                // get loop somehow, e.g. from loop-specific context
    loop.invoke_later([self = std::move(self), message = std::move(message)]() {
        auto &sup = *self;
        sup.put(std::move(message));    // put message into inbound queue
        sup.do_process();               // process inbound queue
    });
}
~~~

How to get loop and what method invoke on it, is the implementation-specific information.
For example, loop reference can be passed on `supervisor` constructor. The `invoke_later`
(alternative names: `postpone`, `CallAfter`, `delay`, `dispatch`) is loop-specific
method how to invoke something on in a thread-safe way. Please note, that `supervisor`
instance is captured via intrusive pointer to make sure it is alive in the loop context
invocations.

The timer-related methods are loop- or application-specific.
