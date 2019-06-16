cmake_minimum_required(VERSION 3.0)
project (rotor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ROTOR_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(BUILD_BOOST_ASIO "Enable building with boost::asio support [default: OFF]" OFF)
option(BUILD_WX         "Enable building with wxWidgets support   [default: OFF]" OFF)
option(BUILD_EXAMPLES   "Enable building examples [default: OFF]"           OFF)
option(BUILD_TESTS      "Enable building tests    [default: OFF]"           OFF)


add_library(rotor
    src/rotor/actor_base.cpp
    src/rotor/error_code.cpp
    src/rotor/message.cpp
    src/rotor/subscription.cpp
    src/rotor/supervisor.cpp
    src/rotor/system_context.cpp
)
target_include_directories(rotor PUBLIC include/)

add_library(rotor_core ALIAS rotor)

if (BUILD_BOOST_ASIO)
    find_package(
        Boost
        COMPONENTS
            date_time
            system
        REQUIRED
    )
    find_package(Threads)

    add_library(rotor_boost
        src/rotor/asio/supervisor_asio.cpp
    )
    target_include_directories(rotor_boost PUBLIC include/)
    target_link_libraries(rotor_boost PUBLIC rotor ${Boost_LIBRARIES} Threads::Threads)
endif()

if (BUILD_WX)
    find_package(wxWidgets COMPONENTS base core REQUIRED)
    include(${wxWidgets_USE_FILE})
    add_library(rotor_wx
        src/rotor/wx/supervisor_wx.cpp
        src/rotor/wx/system_context_wx.cpp
    )
    target_include_directories(rotor_wx PUBLIC include/)
    target_link_libraries(rotor_wx PUBLIC rotor ${wxWidgets_LIBRARIES})
endif()

if (BUILD_TESTS)
    add_subdirectory("tests")
endif()

if (BUILD_EXAMPLES)
    add_subdirectory("examples")
endif()

