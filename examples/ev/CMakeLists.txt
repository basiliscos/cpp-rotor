add_executable(ping-pong-ev ping-pong-ev.cpp)
target_link_libraries(ping-pong-ev rotor_ev)
add_test(ping-pong-ev "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-ev")
