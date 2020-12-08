# Rotor

[reactive]: https://www.reactivemanifesto.org/ "The Reactive Manifesto"
[sobjectizer]: https://github.com/Stiffstream/sobjectizer
[blog-cpp-supervisors]: https://basiliscos.github.io/blog/2019/08/19/cpp-supervisors/ "Trees of Supervisors in C++"
[reliable]: https://en.wikipedia.org/wiki/Reliability_(computer_networking) "reliable"
[request-response]: https://en.wikipedia.org/wiki/Request%E2%80%93response
[blog-cpp-req_res]: https://basiliscos.github.io/blog/2019/10/05/request-response-message-exchange-pattern/

`rotor` is event loop friendly C++ actor micro framework.

[![Travis](https://api.travis-ci.org/basiliscos/cpp-rotor.svg?branch=master)](https://travis-ci.org/basiliscos/cpp-rotor)
[![Build status](https://ci.appveyor.com/api/projects/status/f3a5tnpser7ryj43/branch/master?svg=true)](https://ci.appveyor.com/project/basiliscos/cpp-rotor)
[![codecov](https://codecov.io/gh/basiliscos/cpp-rotor/branch/master/graph/badge.svg)](https://codecov.io/gh/basiliscos/cpp-rotor)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/b9fc6a0fd738473f8fa9084227cd7265)](https://www.codacy.com/manual/basiliscos/cpp-rotor?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=basiliscos/cpp-rotor&amp;utm_campaign=Badge_Grade)
[![license](https://img.shields.io/github/license/basiliscos/cpp-rotor.svg)](https://github.com/basiliscos/cpp-rotor/blob/master/LICENSE)

## features

- minimalistic loop agnostic core
- [erlang-like](https://en.wikipedia.org/wiki/Erlang_(programming_language)#Supervisor_trees) hierarchical supervisors,
[see](https://basiliscos.github.io/blog/2019/08/19/cpp-supervisors/)
- various event loops supported (wx, boost-asio, ev) or planned (uv, gtk, etc.)
- asynchornous message passing interface
- [request-response](https://en.wikipedia.org/wiki/Request%E2%80%93response) messaging with cancellation capabilities,
[see](https://basiliscos.github.io/blog/2019/10/05/request-response-message-exchange-pattern/)
- MPMC (multiple producers mupltiple consumers) messaging, aka pub-sub
- cross-platform (windows, macosx, linux)
- inspired by [The Reactive Manifesto](reactive) and [sobjectizer]

## license

MIT

## documentation

Please read tutorial, design principles and manual [here](https://basiliscos.github.io/cpp-rotor-docs/index.html)

## Changelog

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
