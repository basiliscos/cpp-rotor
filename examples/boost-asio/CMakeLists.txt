include_directories(${rotor_SOURCE_DIR}/src)

add_executable(ping_pong-single-simple ping_pong-single-simple.cpp)
target_link_libraries(ping_pong-single-simple rotor_boost)
add_test(ping_pong-single-simple "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping_pong-single-simple")

add_executable(ping_pong-2-theads ping_pong-2-theads.cpp)
target_link_libraries(ping_pong-2-theads rotor_boost)
add_test(ping_pong-2-theads "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping_pong-2-theads")

add_executable(ping-pong-2-threads-preemt ping-pong-2-threads-preemt.cpp)
target_link_libraries(ping-pong-2-threads-preemt rotor_boost)
add_test(ping-pong-2-threads-preemt "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-2-threads-preemt")
