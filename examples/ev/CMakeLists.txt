add_executable(ping-pong-ev ping-pong-ev.cpp)
target_link_libraries(ping-pong-ev rotor_ev)
add_test(ping-pong-ev "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-ev")

add_executable(ping-pong-req ping-pong-req.cpp)
target_link_libraries(ping-pong-req rotor_ev)
add_test(ping-pong-req "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-req")

add_executable(ping-pong-comp ping-pong-comp.cpp)
target_link_libraries(ping-pong-comp rotor_ev)
add_test(ping-pong-comp "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-comp")

add_executable(pong-registry pong-registry.cpp)
target_link_libraries(pong-registry rotor_ev)
add_test(pong-registry "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pong-registry")
