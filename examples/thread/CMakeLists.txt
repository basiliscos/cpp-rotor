cmake_minimum_required(VERSION 3.15)
find_package(OpenSSL  1.1.1...3.2.0 COMPONENTS Crypto)

if (BUILD_SHARED_LIBS)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../../bin)
endif ()

if (OPENSSL_FOUND)
    add_executable(sha512 sha512.cpp)
    target_link_libraries(sha512 rotor::thread OpenSSL::Crypto)
    add_test(sha512 "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sha512")
endif()

add_executable(ping-pong-spawner ping-pong-spawner.cpp)
target_link_libraries(ping-pong-spawner rotor::thread)
add_test(ping-pong-spawner "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-spawner")

if (NOT ROTOR_BUILD_THREAD_UNSAFE)
    add_executable(ping-pong-thread ping-pong-thread.cpp)
    target_link_libraries(ping-pong-thread rotor::thread)
    add_test(ping-pong-thread "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ping-pong-thread")
endif()
