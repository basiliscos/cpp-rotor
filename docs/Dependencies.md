# Dependencies

[boost-smartptr]: https://www.boost.org/doc/libs/release/libs/smart_ptr/ "Boost Smart Pointers"
[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio/ "Boost Asio"
[wx-widgets]: https://www.wxwidgets.org/ "wxWidgets"

C++ 17 is required to use rotor.

The core dependency `rotor-core` needs intrusive pointer support from [boost-smartptr].

The optional event-loop specific supervisors depend on corresponding loop libraries, i.e.
`rotor-asio` depends on [boost-asio]; the `rotor-wx` depends [wx-widgets].

