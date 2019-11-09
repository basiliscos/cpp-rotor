# Rotor

[reactive]: https://www.reactivemanifesto.org/ "The Reactive Manifesto"
[sobjectizer]: https://github.com/Stiffstream/sobjectizer
[blog-cpp-supervisors]: https://basiliscos.github.io/blog/2019/08/19/cpp-supervisors/ "Trees of Supervisors in C++"
[reliable]: https://en.wikipedia.org/wiki/Reliability_(computer_networking) "reliable"
[request-response]: https://en.wikipedia.org/wiki/Request%E2%80%93response


`rotor` is event loop friendly C++ actor micro framework.

[![Travis](https://img.shields.io/travis/basiliscos/cpp-rotor.svg)](https://travis-ci.org/basiliscos/cpp-rotor)
[![Build status](https://ci.appveyor.com/api/projects/status/f3a5tnpser7ryj43?svg=true)](https://ci.appveyor.com/project/basiliscos/cpp-rotor)
[![codecov](https://codecov.io/gh/basiliscos/cpp-rotor/badge.svg)](https://codecov.io/gh/basiliscos/cpp-rotor)
[![license](https://img.shields.io/github/license/basiliscos/cpp-rotor.svg)](https://github.com/basiliscos/cpp-rotor/blob/master/LICENSE)

## features

- minimalistic loop agnostic core
- various event loops supported (wx, boost-asio, ev) or planned (uv, gtk, etc.)
- asynchornous message passing interface
- MPMC (multiple producers mupltiple consumers) messaging, aka pub-sub
- cross-platform (windows, macosx, linux)
- inspired by [The Reactive Manifesto](reactive) and [sobjectizer]

## license

MIT

## documentation

Please read tutorial, design principles and manual [here](https://basiliscos.github.io/cpp-rotor-docs/index.html)

## Changelog


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
