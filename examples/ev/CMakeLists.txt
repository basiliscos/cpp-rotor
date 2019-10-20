add_executable(ping-pong-ev ping-pong-ev.cpp)
target_link_libraries(ping-pong-ev rotor_ev rotor_asio)
add_test(ping-pong-ev "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-ev")

add_executable(ping-pong-req ping-pong-req.cpp)
target_link_libraries(ping-pong-req rotor_ev rotor_asio)
add_test(ping-pong-req "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-req")
