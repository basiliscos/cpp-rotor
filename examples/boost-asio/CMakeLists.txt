find_package(Boost
    COMPONENTS
        date_time system regex program_options
    REQUIRED)

add_executable(hello_asio hello_asio.cpp)
target_link_libraries(hello_asio rotor_asio ${Boost_LIBRARIES})
add_test(hello_asio "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hello_asio")

add_executable(request-response request-response.cpp)
target_link_libraries(request-response rotor_asio ${Boost_LIBRARIES})
add_test(request-response "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/request-response")

add_executable(request-response-discovery request-response-discovery.cpp)
target_link_libraries(request-response-discovery rotor_asio ${Boost_LIBRARIES})
add_test(request-response-discovery "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/request-response-discovery")

add_executable(ping_pong-single-simple ping_pong-single-simple.cpp)
target_link_libraries(ping_pong-single-simple rotor_asio ${Boost_LIBRARIES})
add_test(ping_pong-single-simple "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping_pong-single-simple")

add_executable(one-shot-lambda-subscriber one-shot-lambda-subscriber.cpp)
target_link_libraries(one-shot-lambda-subscriber rotor_asio ${Boost_LIBRARIES})
add_test(one-shot-lambda-subscriber "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/one-shot-lambda-subscriber")

add_executable(ping_pong-2-theads ping_pong-2-theads.cpp)
target_link_libraries(ping_pong-2-theads rotor_asio ${Boost_LIBRARIES})
add_test(ping_pong-2-theads "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping_pong-2-theads")

add_executable(ping-pong-2-threads-preemt ping-pong-2-threads-preemt.cpp)
target_link_libraries(ping-pong-2-threads-preemt rotor_asio ${Boost_LIBRARIES})
add_test(ping-pong-2-threads-preemt "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-2-threads-preemt")

add_executable(beast-scrapper beast-scrapper.cpp)
target_link_libraries(beast-scrapper rotor_asio ${Boost_LIBRARIES})
add_test(beast-scrapper "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/beast-scrapper")
