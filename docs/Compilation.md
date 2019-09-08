# Compiling & building

## Dependencies

[boost-smartptr]: https://www.boost.org/doc/libs/release/libs/smart_ptr/ "Boost Smart Pointers"
[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio/ "Boost Asio"
[wx-widgets]: https://www.wxwidgets.org/ "wxWidgets"
[ev]: http://software.schmorp.de/pkg/libev.html

C++ 17 is required to use rotor.

The core dependency `rotor-core` needs intrusive pointer support from [boost-smartptr].

The optional event-loop specific supervisors depend on corresponding loop libraries, i.e.
`rotor-asio` depends on [boost-asio]; the `rotor-wx` depends [wx-widgets].

## Compiling

`rotor` uses `cmake` for building; it supports the following building options

- `BUILD_BOOST_ASIO` - build with [boost-asio] support (`off` by default)
- `BUILD_WX` build with [wx-widgets] support (`off` by default)
- `BUILD_EV` build with [libev] support (`off` by default)
- `BUILD_EXAMPLES` build examples (`off` by default)
- `BUILD_TESTS` build tests (`off` by default)
- `BUILD_THREAD_UNSAFE` builds thread-unsafe library (`off` by default)
- `BUILD_DOC` generate documentation (`off` by default, only in release mode)

~~~
git clone https://github.com/basiliscos/cpp-rotor rotor
cd rotor
mkdir build
cd build
cmake --build .. --config Release -DBUILD_BOOST_ASIO=on -DBUILD_WX=on
~~~

