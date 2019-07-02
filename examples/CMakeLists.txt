if (BUILD_BOOST_ASIO)
    add_subdirectory("boost-asio")
endif()

add_executable(hello_loopless hello_loopless.cpp)
target_link_libraries(hello_loopless rotor)
add_test(hello_loopless "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hello_loopless")

add_executable(ping_pong ping_pong.cpp)
target_link_libraries(ping_pong rotor)
add_test(ping_pong "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping_pong")
