cmake_minimum_required(VERSION 3.0)
project (rotor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(
    Boost
    COMPONENTS
        chrono
        coroutine
        filesystem
        regex
        system
        thread
        program_options
    REQUIRED
)

add_executable(rotor src/main.cpp)
target_include_directories(rotor PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(rotor PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(rotor PUBLIC ${Boost_LIBRARIES})
