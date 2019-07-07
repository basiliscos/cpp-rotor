# Rotor

[reactive]: https://www.reactivemanifesto.org/ "The Reactive Manifesto"
[sobjectizer]: https://github.com/Stiffstream/sobjectizer

`rotor` is event loop friendly C++ actor micro framework.

[![Travis](https://img.shields.io/travis/basiliscos/cpp-rotor.svg)](https://travis-ci.org/basiliscos/cpp-rotor)
[![codecov](https://codecov.io/gh/basiliscos/cpp-rotor/badge.svg)](https://codecov.io/gh/basiliscos/cpp-rotor)
[![license](https://img.shields.io/github/license/basiliscos/cpp-rotor.svg)](https://github.com/basiliscos/cpp-rotor/blob/master/LICENSE)

## features

- minimalistic loop agnostic core
- various event loops supported (wx, boost-asio) or planned (ev, gtk, etc.)
- asynchornous message passing interface
- MPMC (multiple producers mupltiple consumers) messaging, aka pub-sub
- cross-platform (windows, linux)
- inspired by [The Reactive Manifesto](reactive) and [sobjectizer]

## license

MIT

## documentation

Please read tutorial, design principles and manual [here](https://basiliscos.github.io/cpp-rotor-docs/index.html)
