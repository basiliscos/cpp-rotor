include_directories(${rotor_SOURCE_DIR}/src)

find_package(
    Boost
    COMPONENTS
        date_time
        system
    REQUIRED
)
find_package(Threads)

set(rotor_EXAMPLES_LIBS rotor_boost rotor ${Boost_LIBRARIES} Threads::Threads)

add_executable(ping_pong-single-simple ping_pong-single-simple.cpp)
target_link_libraries(ping_pong-single-simple ${rotor_EXAMPLES_LIBS})
add_test(ping_pong-single-simple "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping_pong-single-simple")
