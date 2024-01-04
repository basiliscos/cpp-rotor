cmake_minimum_required(VERSION 3.23)

if (BUILD_BOOST_ASIO)
    add_subdirectory("boost-asio")
endif()

if (BUILD_EV)
    add_subdirectory("ev")
endif()

if (BUILD_BOOST_ASIO AND BUILD_EV AND NOT BUILD_THREAD_UNSAFE)
    add_executable(ping-pong-ev_and_asio ping-pong-ev_and_asio.cpp)
    target_link_libraries(ping-pong-ev_and_asio rotor_ev rotor_asio)
    add_test(ping-pong-ev_and_asio "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-ev_and_asio")
endif()

if (BUILD_THREAD)
    add_subdirectory("thread")
endif()


add_executable(autoshutdown autoshutdown.cpp)
target_link_libraries(autoshutdown rotor)
add_test(autoshutdown "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/autoshutdown")

add_executable(hello_loopless hello_loopless.cpp)
target_link_libraries(hello_loopless rotor)
add_test(hello_loopless "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hello_loopless")

add_executable(escalate-failure escalate-failure.cpp)
target_link_libraries(escalate-failure rotor)
add_test(escalate-failure "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/escalate-failure")

add_executable(ping_pong ping_pong.cpp)
target_link_libraries(ping_pong rotor)
add_test(ping_pong "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping_pong")

add_executable(ping_pong-lambda ping_pong-lambda.cpp)
target_link_libraries(ping_pong-lambda rotor)
add_test(ping_pong-lambda "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping_pong-lambda")

add_executable(pub_sub pub_sub.cpp)
target_link_libraries(pub_sub rotor)
add_test(pub_sub "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pub_sub")

