# Rotor

[reactive]: https://www.reactivemanifesto.org/ "The Reactive Manifesto"
[sobjectizer]: https://github.com/Stiffstream/sobjectizer

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

### 0.03 (25-Aug-2019)

 - [improvement] locality notion was introduced, which led to possibilty
to build superving trees, see [superving-trees]]
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
