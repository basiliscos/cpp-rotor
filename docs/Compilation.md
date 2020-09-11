# Compiling & building

## Dependencies

[boost-smartptr]: https://www.boost.org/doc/libs/release/libs/smart_ptr/ "Boost Smart Pointers"
[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio/ "Boost Asio"
[wx-widgets]: https://www.wxwidgets.org/ "wxWidgets"
[ev]: http://software.schmorp.de/pkg/libev.html

C++ 17 is required to use rotor.

The core dependency `rotor-core` needs intrusive pointer support from [boost-smartptr] 
and `boost::posix_time::time_duration`. (That might be changed in future, PRs are welcome).

The optional event-loop specific supervisors depend on corresponding loop libraries, i.e.
`rotor-asio` depends on [boost-asio]; the `rotor-wx` depends [wx-widgets].

## Compiling

`rotor` uses `cmake` for building; it supports the following building options

- `BUILD_BOOST_ASIO` - build with [boost-asio] support (`off` by default)
- `BUILD_WX` build with [wx-widgets] support (`off` by default)
- `BUILD_EV` build with [libev] support (`off` by default)
- `BUILD_EXAMPLES` build examples (`off` by default)
- `BUILD_TESTS` build tests (`off` by default)
- `BUILD_DOC` generate doxygen documentation (`off` by default, only for release builds)
- `BUILD_THREAD_UNSAFE` builds thread-unsafe library (`off` by default)
- `ROTOR_DEBUG_DELIVERY` allow runtime messages inspection (`off` by default, enabled by default for debug builds)

~~~
git clone https://github.com/basiliscos/cpp-rotor rotor
cd rotor
mkdir build
cd build
cmake --build .. --config Release -DBUILD_BOOST_ASIO=on -DBUILD_WX=on
~~~

## Adding rotor into a project (modern way)

Your `CMakeLists.txt` should have something like

~~~
include(FetchContent)

set(BUILD_BOOST_ASIO ON CACHE BOOL "with asio") # pick options you need
FetchContent_Declare(
    rotor
    GIT_REPOSITORY https://github.com/basiliscos/cpp-rotor.git
    GIT_TAG v0.07
)
FetchContent_MakeAvailable(rotor)

target_include_directories(my_target PUBLIC ${rotor_SOURCE_DIR}/include)
target_link_libraries(my_target rotor::asio)
~~~

## Adding rotor into a project (classical way)

~~~
git clone https://github.com/basiliscos/cpp-rotor.git external/rotor --branch=v0.07
~~~

Your `CMakeLists.txt` should have something like

~~~
set(BUILD_BOOST_ASIO ON CACHE BOOL "with asio")
add_subdirectory("lib/cpp-rotor")

target_include_directories(my_target PUBLIC
    ${PROJECT_SOURCE_DIR}/external/rotor/include
)
target_link_libraries(aramis_lib rotor::asio)
~~~
