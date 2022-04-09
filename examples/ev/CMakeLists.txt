cmake_minimum_required(VERSION 3.8)
add_executable(ping-pong-ev ping-pong-ev.cpp)
target_link_libraries(ping-pong-ev rotor_ev)
add_test(ping-pong-ev "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-ev")

find_package(Threads)

if (Threads_FOUND AND NOT BUILD_THREAD_UNSAFE)
    add_executable(ping-pong-ev-2-threads ping-pong-ev-2-threads.cpp)
    target_link_libraries(ping-pong-ev-2-threads rotor_ev Threads::Threads)
    add_test(ping-pong-ev-2-threads "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-ev-2-threads")
endif()

add_executable(ping-pong-req ping-pong-req.cpp)
target_link_libraries(ping-pong-req rotor_ev)
add_test(ping-pong-req "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-req")

add_executable(ping-pong-comp ping-pong-comp.cpp)
target_link_libraries(ping-pong-comp rotor_ev)
add_test(ping-pong-comp "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-comp")

add_executable(pong-registry pong-registry.cpp)
target_link_libraries(pong-registry rotor_ev)
add_test(pong-registry "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pong-registry")
