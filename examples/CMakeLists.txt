cmake_minimum_required(VERSION 3.15)

if (BUILD_SHARED_LIBS)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin)
endif ()

if (ROTOR_BUILD_ASIO)
    add_subdirectory("boost-asio")
endif()

if (ROTOR_BUILD_EV)
    add_subdirectory("ev")
endif()

if (ROTOR_BUILD_ASIO AND ROTOR_BUILD_EV AND NOT ROTOR_BUILD_THREAD_UNSAFE)
    add_executable(ping-pong-ev_and_asio ping-pong-ev_and_asio.cpp)
    target_link_libraries(ping-pong-ev_and_asio rotor_ev rotor_asio)
    add_test(ping-pong-ev_and_asio "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-ev_and_asio")
endif()

if (ROTOR_BUILD_THREAD)
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

if (ROTOR_BUILD_FLTK)
    find_package(JPEG REQUIRED)

    add_executable(ping-pong-fltk ping-pong-fltk.cpp)
    target_link_libraries(ping-pong-fltk
        rotor::fltk
        $<$<PLATFORM_ID:Linux>:jpeg>
        $<$<PLATFORM_ID:Linux>:png>
    )
    add_test(ping-pong-fltk "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-fltk")

    if (ROTOR_BUILD_THREAD)
        add_executable(ping-pong-fltk_and_thread ping-pong-fltk_and_thread.cpp)
        target_link_libraries(ping-pong-fltk_and_thread
            rotor::fltk rotor::thread
            $<$<PLATFORM_ID:Linux>:jpeg>
            $<$<PLATFORM_ID:Linux>:png>
        )
        add_test(ping-pong-fltk_and_thread "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-fltk_and_thread")
    endif()
endif()
