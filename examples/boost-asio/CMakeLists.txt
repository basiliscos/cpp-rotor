cmake_minimum_required(VERSION 3.23)
find_package(Boost
    COMPONENTS
        date_time system regex program_options
    REQUIRED)

if (BUILD_SHARED_LIBS)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../../bin)
endif ()

if (WIN32)
    add_compile_definitions(
        _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
        _WIN32_WINNT=0x600
    )
endif()

add_executable(hello_asio hello_asio.cpp)
target_link_libraries(hello_asio rotor_asio ${Boost_LIBRARIES})
add_test(hello_asio "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hello_asio")

add_executable(request-response request-response.cpp)
target_link_libraries(request-response rotor_asio ${Boost_LIBRARIES})
add_test(request-response "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/request-response")

add_executable(request-response-discovery request-response-discovery.cpp)
target_link_libraries(request-response-discovery rotor_asio ${Boost_LIBRARIES})
add_test(request-response-discovery "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/request-response-discovery")

add_executable(ping-pong-single-simple ping-pong-single-simple.cpp)
target_link_libraries(ping-pong-single-simple rotor_asio ${Boost_LIBRARIES})
add_test(ping-pong-single-simple "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-single-simple")

add_executable(ping-pong-timer ping-pong-timer.cpp)
target_link_libraries(ping-pong-timer rotor_asio ${Boost_LIBRARIES})
add_test(ping-pong-timer "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-timer")

add_executable(one-shot-lambda-subscriber one-shot-lambda-subscriber.cpp)
target_link_libraries(one-shot-lambda-subscriber rotor_asio ${Boost_LIBRARIES})
add_test(one-shot-lambda-subscriber "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/one-shot-lambda-subscriber")

if (NOT BUILD_THREAD_UNSAFE)
    add_executable(ping-pong-2-threads ping-pong-2-threads.cpp)
    target_link_libraries(ping-pong-2-threads rotor_asio ${Boost_LIBRARIES})
    add_test(ping-pong-2-threads "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-2-threads")

    add_executable(ping-pong-2-threads-preemt ping-pong-2-threads-preemt.cpp)
    target_link_libraries(ping-pong-2-threads-preemt rotor_asio ${Boost_LIBRARIES})
    add_test(ping-pong-2-threads-preemt "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-2-threads-preemt")
endif()

add_executable(beast-scrapper beast-scrapper.cpp)
target_link_libraries(beast-scrapper rotor_asio ${Boost_LIBRARIES})
add_test(beast-scrapper "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/beast-scrapper")
