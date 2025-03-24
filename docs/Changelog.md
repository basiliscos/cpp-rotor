# Changelog

[superving-trees]: https://basiliscos.github.io/blog/2019/08/19/cpp-supervisors/
[reliable]: https://en.wikipedia.org/wiki/Reliability_(computer_networking) "reliable"
[request-response]: https://en.wikipedia.org/wiki/Request%E2%80%93response

### 0.34 (xx-xxx-2025)
- [workaround, fltk] more realiable message delivery for fltk backend. Fltk
might "forget" to invoke scheduled `awakes` after shutdown process, which leads
to minor memory leaks

### 0.33 (26-Jan-2025)
 - [cmake, bugfix, win32] add `ws2_32` lib for rotor_asion win32 build

### 0.32 (18-Dec-2024)
- [feature] added `make_routed_message()` free function.
  The function is used for "synchronized" message post-processing, i.e. once a
  message has been delivered  and processed by all recipients (can be zero),
  then it is routed to the specifed address to do cleanup.

Example:
1. an db-actor and opens transaction and reads data (i.e. as `std::string_view`s, owned by db)
2. actors sends broadcast message to all interesting parties to deserialized data
3. **after** the step 2 is finished the db-actor closes transaction and releases acquired resources.

The `make_routed_message` is needed to perform recipient-agnostic 3rd step.

The alternative is to create a copy (snapshot) of data (i.e. `std::string`
instead of `std::string_view`), but that  seems redundant.

 - [improvement, breaking] add `void*` parameter to `message_visitor_t`

### 0.31 (18-Oct-2024)
 - [bugfix, fltk] more realiable message delivery for fltk backend

### 0.30 (23-Apr-2024)
 - [feature] added [fltk](https://www.fltk.org/)-backend
 - [feature, conan] `enable_fltk` option which add fltk-support
 - [example] added `/examples/ping-pong-fltk.cpp` and
`examples/ping-pong-fltk_and_thread.cpp`
 - [bugfix] [wx-backend](https://wxwidgets.org/) building and testing
 - [improvement, breaking] output directories are set to `bin` for cmake
 - [improvement, breaking] `actor_base_t::make_error()` is marked as const

### 0.29 (24-Feb-2024)
 - [bugfix] fix segfault in delivery plugin in debugging mode
(try to set env `ROTOR_INSPECT_DELIVERY=99` to see)

### 0.28 (22-Jan-2024)
 - [cmake, bugfix] add missing header into installation

### 0.27 (21-Jan-2024)
 - [feature] new interface `message_visitor_t`
 - [feature] new interface `message_stringifier_t` and the default implementation
`default_stringifier_t` which allows to dump messages. It is not a production but
a diagnostic/debug tool, due to performance restrictions.
 - [feature] `system_context_t` provides a reference to default `message_stringifier_t`;
it is possible to have a custom one
 - [feature, breaking] `extended_error_t` holds a reference to a request message,
which caused an error
 - [examples, tests, win32] fix `ev` examples and tests
 - [example] modernize `examples/thread/sha512.cpp` to use recent `openssl` version
 - [breaking] cmake requirements are lowered to `3.15`
 - [breaking] fix minor compilation warnings

### 0.26 (08-Jan-2024)
 - [feature] `start_timer` callback not only method, but any invocable
 - [feature, conan] `enable_ev` option which add `libev`
 - [breaking, conan] `boost` minimum version `1.83.0`
 - [testing, conan] remove `catch2` from sources and make it dependencies
 - [bugfix, breaking] make plugins more dll-friendly
 - [breaking] `cmake` minimum version `3.23`
 - [breaking] rename ```registry_t::revese_map_t revese_map``` -> ```registry_t::reverse_map_t reverse_map```
 - [breaking] rename struct ```cancelation_t``` -> ```cancellation_t```
 - [doc] fix multiple typos

### 0.25 (26-Dec-2022)
 - [bugfix] avoid response messages loose their order relative to regular message
 - [bugfix, example] add missing header

### 0.24 (04-Jun-2022)
 - [feature] improve inter-thread messaging performance up to 15% by using `boost::unordered_map`
instead of `std::unordered_map`
 - [bugfix, breaking] avoid introducing unnecessary event loops latency by intensive polling of
rotor queues; affects `asio` and `ev` loops
 - [bugfix] `registry_plugin_t`, allow to discover aliased services (#46)

### 0.23 (23-Apr-2022)
 - [bugfix] fix compilation issues of `messages.cpp` on some platforms
 - [bugfix, msvc] fix compilation issues of `registry` plugin for `shared` library
on msvc-16+

### 0.22 (21-Apr-2022)
 - [feature] possibly to install via [conan center](https://conan.io/center/rotor)
 - [feature, breaking] possibility to build `rotor` as shared library
 - [feature] add shutdown flag checker (see [my blog](https://basiliscos.github.io/blog/2022/04/09/rotor-v022-and-thread-unsafety/))
 - [bugfix] requests do not outlive actors (i.e. they are cancelled on `shutdown_finish`)
 - [example] there is my another open-source project [syncspirit](https://github.com/basiliscos/syncspirit),
which uses `rotor` under hood. I recommend to look at it, if the shipped examples are too-trivial, and
don't give you an architectural insight of using `rotor`.

### 0.21 (25-Mar-2022)
 - [improvement] preliminary support of [conan](https://conan.io) package manager
 - [bugfix] fix compilation warnings on Windows/MSVC
 - [bugfix] add missing header for `rotor::thread` installation

### 0.20 (20-Feb-2022)
 - [improvement] superviser can create `spawner`, which has a policy to auto-spawns
new actor instance if previous instance has been shutdown. This is similar to
escalate_failure [supervising in erlang](https://www.erlang.org/doc/design_principles/sup_princ.html),
see [dedicated article](https://basiliscos.github.io/blog/2022/02/20/supervising-in-c-how-to-make-your-programs-reliable)
 - [improvement] actor can now `autoshutdown_supervisor()`, when it shutdown
 - [improvement] actor can now `escalate_failure()`, i.e. trigger shutdown own supervisor
when it finished with failure
 - [improvement] messages delivery order is persevered per-locality (see issue #41)
 - [example] `examples/thread/ping-pong-spawner` (new)
 - [example] `examples/autoshutdown` (new)
 - [example] `examples/escalate-failure` (new)
 - [documentation] updated `Design principles`
 - [documentation] updated `Examples`
 - [documentation] updated `Introduction`

### 0.19 (31-Dec-2021)
 - [improvement] performance improvement in inter-thread (+20%) and cross-thread messaging
 - [bugfix] supervisor does not shut self down in rare conditions, when it fails to initialize self
 - [bugfix] link_server plugin should ignore unlink_notifications
 - [bugfix] avoid cycle (i.e. memleak) in rare cases when supervisor is shutdown, but an external
message arrives for processing

### 0.18 (03-Dec-2021)
- [improvement] add `static_assert` for `noexcept` check of a hanler signature
- [improvement] add [gitee](https://gitee.com/basiliscos/cpp-rotor) mirror
- [bugfix] fix potential use-after-free in `ev` backend


### 0.17 (23-Oct-2021)
- [bugfix] fix installation issues with cmake (thanks to @melpon)
- [bugfix] fix missing header (thanks to @melpon)
- [ci] drop travis-ci in the sake of circle-ci


### 0.16 (22-Aug-2021)
- [improvement] significant message throughtput increase for `std::thread`, `boost-asio`
and `ev` backends (upto 5x times)
- [improvement] `extended_error` can now access to root reason
- [improvement] delivery plugin in debug mode dumps discovery requests and responses
- [improvement/breaking] more details on fatal error (`system_context`)
- [example] `examples/thread/ping-pong-thread.cpp` (new)
- [example] `examples/ev/ping-pong-ev-2-threads` (new)

### 0.15 (02-Apr-2021)
 - [bugfix] `lifetime_plugin_t` do not unsubscribe from foreign to me subscriptions
 - [bugfix] `foreigners_support_plugin_t` more safely deliver a message for a foreign
subscriber (actor)

### 0.14 (20-Feb-2021)
- the dedicated article with highlights: [en](https://habr.com/en/post/543364/) and
[ru](https://habr.com/ru/post/543362/)
 - [improvement] actor identity has been introduced. It can be configured or generated via
`address_maker` plugin
 - [improvement] `actor::do_shutdown()` - optionally takes shutdown reason
 - [improvement/breaking] instead of using `std::error_code` the `extended_error` class
is used. It wraps `std::error_code`, provides string context and pointer to the next
`extended_error` cause. This greatly simplifies error tracking of errors. Every response
now contains `ee` field instead of `ec`.
 - [improvement] `actor` has shutdown reason (in form of `extended_error` pointer)
 - [improvement] delivery plugin in debug mode it dumps shutdown reason in shutdown trigger
 messages
 - [improvement] actor identity has `on_unlink` method to get it know, when it has been
unlinked from server actor
 - [improvement] add `resources` plugin for supervisor
 - [breaking] all responses now have `extended_error` pointer instead of `std::error_code`
 - [breaking] `shutdown_request_t` and `shutdown_trigger_t` messages how have
shutdown reason (in form of `extended_error` pointer)
 - [bugfix] `link_client_plugin_t` do not invoke custom callback, before erasing request
in case of failure
 - [bugfix] `child_manager_plugin_t` reactivate self if a child was created from other
plugin.
 - [bugfix] `registry actor` incorrectly resolves postponed requests to wrong addresses

### 0.13 (26-Dec-2020)
 - [improvement] delivery plugin in debug mode dumps discarded messages
 - [breaking] `state_response_t` has been removed
 - [bugfix] allow to acquire & release resources in via `resources_plugin_t`, during
other plugin configuration
 - [bugfix] foreigners_support_plugin_t did not deactivated self properly, caused
assertion fail on supervisor shutdown, when there was foreign subscriptions
 - [bugfix] link_client_plugin_t did not notified linked server-actors, if its actor is
going to shutdown; now server-actors are requested to unlink
 - [bugfix] starter_plugin_t sometimes crashed when subscription confirmation message
arrives when actor is in non-initializing phase (i.e. shutting down)
 - [bugfix] root supervisor is not shutdown properly when it is linked as "server"

### 0.12 (08-Dec-2020)
- [improvement] added `std::thread` backend (supervisor)
- [bugfix] active timers, if any, are cancelled upon actor shutdown finish
- [bugfix] supervisor shutdown message is lost in rare cases right after
child actor start
- [example] `examples/thread/sha512.cpp` (new)
- [documentation] updated `Event loops & platforms`
- [documentation] updated `Patterns` with `Blocking I/O multiplexing`
- [deprecated] state_response_t, state_request_t will be removed in v0.13


### 0.11 (20-Nov-2020)
- [improvement] when supervisor shuts self down due to child init failure,
the supervisor init error code is "failure escalation"
- [documentation] updated `Advanced examples`,
- [bugfix] when actor shuts self down all its timers are properly
cancelled
- [bugfix] in rare case supervisor starts, event if child failed to init
- [bugfix] asio: more correct timers cancellation implementation
- [bugfix] ev: more correct shutdown (avoid memory leaks in rare cases)

### 0.10 (09-Nov-2020)
- [improvement/breaking] Generic timers interface
- [improvement] Request cancellation support
- [improvement] added `make_response` methods when message should be created, but
send later delayed
- [improvement] more debug information in message delivery plugin
- [documentation] Integration with event loops
- [documentation] Requests cancellation and timers are demonstrated in the
[Advanced Examples](docs/Examples.md) section
- [example] `examples/boost-asio/ping-pong-timer.cpp` (new)
- [example] `examples/boost-asio/beast-scrapper.cpp` (updated)
- [bugfix] avoid double configuration of a plugin in certain cases when interacting
with resources plugin
- [bugfix] more correct cmake installation (thanks to Jorge López Tello, @LtdJorge)

### 0.09 (03-Oct-2020)
- the dedicated article with highlights: [en](https://habr.com/ru/company/crazypanda/blog/522588/) and
[ru](https://habr.com/ru/company/crazypanda/blog/522892/)
- [improvement] rewritten whole documentation
- [improvement/breaking] plugin system where introduced for actors instead of
behaviors
- [improvement] `actor_config_t` was introduced, which now holds pointer to
supervisor, init and shutdown timeouts
- [improvement] `builder` pattern was introduced to simplify actors construction
- [breaking] `supervisor_config_t` was changed (inherited from `actor_config_t`)
- [breaking] `actor_base_t` and `supervisor_t` constructors has changed - now
appropriate config is taken as single parameter. All descendant classes should
be changed
- [breaking] if a custom config type is used for actors/supervisors, they
should define `config_t` inside the class, and templated `config_builder_t`.
- [breaking] supervisor in actor is now accessible via pointer instead of
reference
- [bugfix] `supervisor_ev_t` not always correctly released EV-resources, which
lead to leak
- [bugfix] `actor_base_t` can be shutted down properly even if it did not
started yet


### 0.08 (12-Apr-2020)

- [bugfix] message's arguments are more correctly forwarded
- [bugfix] actor's arguments are more correctly forwarded in actor's
creation in`rotor::supervisor_t` and `rotor::asio::supervisor_asio_t`
- [bugfix] `rotor::asio::forwarder_t` now more correctly dispatches
`boost::asio` events to actor methods; e.g. it works correctly now with
`async_accept` method of `socket_acceptor`


### 0.07 (02-Apr-2020)

- [improvement] more modern cmake usage


### 0.06 (09-Nov-2019)

- [improvement] registry actor was added to allow via name/address runtime
matching do services discovery
- [improvement, breaking] minor changes in supervisor behavior: now it
is considered initialized when all its children confirmed initialization
- [improvement] `supervisor_policy_t` was introduced to control supervisor
behavior on a child-actor startup failure
- [example] `examples/ev/pong-registry.cpp` how to use registry
- [doc] patterns/Registry was added

### 0.05 (22-Sep-2019)

- [improvement] response can be inherited from `rotor::arc_base`, to allow
forwarding requests without copying it (i.e. just intrusive pointer is created)
- [example] `examples/boost-asio/beast-scrapper.cpp` has been added; it
demonstrates an app with pool of actor workers with request-response forwarding

### 0.04 (14-Sep-2019)

- [improvement] the [request-response] approach is integrated to support basic
[reliable] messaging: response notification failure will be delivered,
if the expected response will not arrive in-time
- [improvement] lambda subscribers are supported
- [improvement] actor behavior has been introduced to offload actor's
interface
- [breaking] supervisor is constructed with help of `supervisor_config_t`,
which contains shutdown timeout value
- [breaking] supervisor does not spawns timeout timer for overall shutdown
procedure, instead per-child timers are spawned. The root supervisor
the same way monitors child-supervisor shut down
- [breaking] supervisor `create_actor` method now takes child max
init time value. If it does not confirm, the child actor will be asked
for shut down.
- [breaking] shutdown request sent to an child actor now timeout-tracked
by supervisor. The message type has changed to `message::shutdown_request_t`
- [breaking] init request sent to an child actor now timeout-tracked
by supervisor. The message type has changed to `message::init_request_t`
- [breaking] actor's state request message type now `message::state_request_t`,
which follows the generic request/response pattern. The response type
is now `message::state_response_t`.
- [breaking] {asio, ev, ws} supervisor configs are renamed to have
corresponding suffix.

### 0.03 (25-Aug-2019)

 - [improvement] locality notion was introduced, which led to possibility
to build supervising trees, see [blog-cpp-supervisors]
 - [breaking] the `outbound` field in `rotor::supervisor_t` was renamed just to `queue`
 - [breaking] `rotor::address_t` now contains `const void*` locality
 - [breaking] `rotor::asio::supervisor_config_t` now contains
`std::shared_ptr` to `strand`, instead of creating private strand
for each supervisor
 - [bugfix] redundant `do_start()` method in `rotor::supervisor_t` was
removed, since supervisor now is able to start self after completing
initialization.
 - [bugfix] `rotor::supervisor_t` sends `initialize_actor_t` to self
to advance own state to `INITIALIZED` via common actor mechanism,
instead of changing state directly on early initialization phase
(`do_initialize`)
 - [bugfix] `rotor::asio::forwarder_t` now more correctly dispatches
boost::asio events to actor methods
 - [bugfix] `rotor::ev::supervisor_ev_t` properly handles refcounter

### 0.02 (04-Aug-2019)

 - Added libev support

### 0.01 (24-Jul-2019)

 - Initial version
