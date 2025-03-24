# Compiling & building

## Conan

The recommended way to install [rotor](https://github.com/basiliscos/cpp-rotor/) is to
use [conan](https://conan.io/center/rotor) package manager.

Among the [various ways](https://docs.conan.io/en/latest/using_packages.html) to
[include rotor](https://conan.io/center/rotor?tab=recipe) in a project, but the
most trivial one is to just check install it to local conan cache via:

~~~bash
conan install rotor/0.26
~~~

Please note, that it might take some some time before the new `rotor` release
appears in [conan center](https://conan.io/center/rotor) as this is intentionally
non-automated and human-supervised process for inclusion.

## Dependencies

[boost-smartptr]: https://www.boost.org/doc/libs/release/libs/smart_ptr/ "Boost Smart Pointers"
[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio/ "Boost Asio"
[wx-widgets]: https://www.wxwidgets.org/ "wxWidgets"
[libev]: http://software.schmorp.de/pkg/libev.html "libev"

C++ 17 is required to use rotor.

The core dependency `rotor-core` needs intrusive pointer support from [boost-smartptr] 
and `boost::posix_time::time_duration`. (That might be changed in future, PRs are welcome).

The optional event-loop specific supervisors depend on corresponding loop libraries, i.e.
`rotor-asio` depends on [boost-asio]; the `rotor-wx` depends [wx-widgets].

## Compiling

`rotor` uses `cmake` for building; it supports the following building options

- `ROTOR_BUILD_ASIO` - build with [boost-asio] support (`off` by default)
- `ROTOR_BUILD_WX` build with [wx-widgets] support (`off` by default)
- `ROTOR_BUILD_EV` build with [libev] support (`off` by default)
- `ROTOR_BUILD_EXAMPLES` build examples (`off` by default)
- `ROTOR_BUILD_DOC` generate doxygen documentation (`off` by default, only for release builds)
- `ROTOR_BUILD_THREAD_UNSAFE` builds thread-unsafe library (`off` by default). Enable this option if you are sure, that
rotor-objects (i.e. messages and actors) are accessed only from single thread.
- `ROTOR_DEBUG_DELIVERY` allow runtime messages inspection (`off` by default, enabled by default for debug builds)

~~~bash
git clone https://github.com/basiliscos/cpp-rotor rotor
cd rotor
mkdir build
cd build
cmake --build .. --config Release -DROTOR_BUILD_ASIO=on -DROTOR_BUILD_WX=on
~~~

## Adding rotor into a project (modern way)

Your `CMakeLists.txt` should have something like

~~~cmake
include(FetchContent)

set(ROTOR_BUILD_ASIO ON CACHE BOOL "with asio") # pick options you need
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

~~~bash
git clone https://github.com/basiliscos/cpp-rotor.git external/rotor lib/rotor --branch=v0.09
~~~

Your `CMakeLists.txt` should have something like

~~~cmake
set(ROTOR_BUILD_ASIO ON CACHE BOOL "with asio")
add_subdirectory("lib/rotor")

target_include_directories(my_target PUBLIC
    ${PROJECT_SOURCE_DIR}/lib/rotor/include
)
target_link_libraries(my_lib rotor::asio)
~~~
