# Rotor

[reactive]: https://www.reactivemanifesto.org/ "The Reactive Manifesto"
[sobjectizer]: https://github.com/Stiffstream/sobjectizer
[blog-cpp-supervisors]: https://basiliscos.github.io/blog/2019/08/19/cpp-supervisors/ "Trees of Supervisors in C++"
[reliable]: https://en.wikipedia.org/wiki/Reliability_(computer_networking) "reliable"
[request-response]: https://en.wikipedia.org/wiki/Request%E2%80%93response
[blog-cpp-req_res]: https://basiliscos.github.io/blog/2019/10/05/request-response-message-exchange-pattern/

`rotor` is event loop friendly C++ actor micro framework,
    [github](https://github.com/basiliscos/cpp-rotor)
    [gitee](https://gitee.com/basiliscos/cpp-rotor)

[![https://t.me/cpp_rotor](https://raw.githubusercontent.com/wiki/erthink/libmdbx/img/telegram.png)](https://t.me/cpp_rotor)
[![CircleCI](https://circleci.com/gh/basiliscos/cpp-rotor.svg?style=svg)](https://circleci.com/gh/basiliscos/cpp-rotor)
[![appveyor](https://ci.appveyor.com/api/projects/status/f3a5tnpser7ryj43/branch/master?svg=true)](https://ci.appveyor.com/project/basiliscos/cpp-rotor)
[![codecov](https://codecov.io/gh/basiliscos/cpp-rotor/branch/master/graph/badge.svg)](https://codecov.io/gh/basiliscos/cpp-rotor)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/b9fc6a0fd738473f8fa9084227cd7265)](https://www.codacy.com/manual/basiliscos/cpp-rotor?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=basiliscos/cpp-rotor&amp;utm_campaign=Badge_Grade)
[![license](https://img.shields.io/github/license/basiliscos/cpp-rotor.svg)](https://github.com/basiliscos/cpp-rotor/blob/master/LICENSE)

## features

- minimalistic loop agnostic core
- [erlang-like](https://en.wikipedia.org/wiki/Erlang_(programming_language)#Supervisor_trees) hierarchical supervisors,
see [this](https://basiliscos.github.io/blog/2022/02/20/supervising-in-c-how-to-make-your-programs-reliable)
and [this](https://basiliscos.github.io/blog/2019/08/19/cpp-supervisors/)
- various event loops supported (wx, boost-asio, ev) or planned (uv, gtk, etc.)
- asynchornous message passing interface
- [request-response](https://en.wikipedia.org/wiki/Request%E2%80%93response) messaging with cancellation capabilities,
[see](https://basiliscos.github.io/blog/2019/10/05/request-response-message-exchange-pattern/)
- MPMC (multiple producers multiple consumers) messaging, aka pub-sub
- cross-platform (windows, macosx, linux)
- inspired by [The Reactive Manifesto](reactive) and [sobjectizer]

## performance


 inter-thread messaging (1)   | cross-thread messaging (2)
------------------------------|-------------------------
  ~23.5M messages/second      | ~ 2.5M messages/second


Setup: Intel Core i7-8550U, Void Linux 5.13.

(1) Backend-independent; Can be measured with `examples/boost-asio/ping-pong-single-simple`, `examples/ev/ping-pong-ev`.

(2) Does not apply to wx-backend; can be measured with  `examples/thread/ping-pong-thread`, 
`examples/boost-asio/ping-pong-2-theads`, `examples/ev/ping-pong-ev-2-threads`.


## license

MIT

## documentation

Please read tutorial, design principles and manual [here](https://basiliscos.github.io/cpp-rotor-docs/index.html)

## Changelog

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
 - [improvement] messages delivery order is preseverd per-locality (see issue #41)
 - [example] `examples/thread/ping-pong-spawner` (new)
 - [example] `examples/autoshutdown` (new)
 - [example] `examples/escalate-failure` (new)
 - [documentation] updated `Design principes`
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
`extended_error` cause. This greatly simplfies error tracking of errors. Every response
now contains `ee` field instead of `ec`.
 - [improvement] `actor` has shutdown reason (in form of `extended_error` pointer)
 - [improvement] delivery plugin in debug mode it dumps shutdown reason in shutdown trigger
 messages
 - [improvement] actor identity has `on_unlink` method to get it know, when it has been
unlinked from server actor
 - [improvement] add `resources` plugin for supervisor
 - [breaking] all responses now have `extended_error` pointer instread of `std::error_code`
 - [breaking] `shutdown_request_t` and `shutdown_trigger_t` messages how have
shutdown reason (in form of `extended_error` pointer)
 - [bugfix] `link_client_plugin_t` do not invoke custom callback, before erasing request
in case of failure
 - [bugfix] `child_manager_plugin_t` reactivate self if a child was created from other
plugin.
 - [bugfix] `registy actor` incorrectly resolves postponed requests to wrong addresses

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
- [breaking] supervisor in actor is now accessibe via pointer instead of
refence
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

- [improvement] registy actor was added to allow via name/address runtime
matching do services discovery
- [improvement, breaking] minor changes in supervisor behavior: now it
is considered initialied when all its children confirmed initialization
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
- [improvement] lambda subscribiers are supported
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

 - [improvement] locality notion was introduced, which led to possibilty
to build superving trees, see [blog-cpp-supervisors]
 - [breaking] the `outbound` field in `rotor::supervisor_t` was renamed just to `queue`
 - [breaking] `rotor::address_t` now contains `const void*` locality
 - [breaking] `rotor::asio::supervisor_config_t` now contains
`std::shared_ptr` to `strand`, instead of creating private strand
for each supervisor
 - [bugfix] redundant `do_start()` method in `rotor::supervisor_t` was
removed, since supervisor now is able to start self after compliting
initialization.
 - [bugfix] `rotor::supervisor_t` sends `initialize_actor_t` to self
to advance own state to `INITIALIZED` via common actor mechanism,
instead of changeing state directly on early initialization phase
(`do_initialize`)
 - [bugfix] `rotor::asio::forwarder_t` now more correctly dispatches
boost::asio events to actor methods
 - [bugfix] `rotor::ev::supervisor_ev_t` properly handles refcounter

### 0.02 (04-Aug-2019)

 - Added libev support

### 0.01 (24-Jul-2019)

Initial version
