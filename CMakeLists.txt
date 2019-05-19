cmake_minimum_required(VERSION 3.0)
project (rotor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(
    Boost
    COMPONENTS
        chrono
        coroutine
        filesystem
        regex
        system
        thread
        program_options
    REQUIRED
)

set(ROTOR_INCLUDE_DIRS ${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(rotor src/main.cpp)
target_include_directories(rotor PUBLIC ${ROTOR_INCLUDE_DIRS})
target_link_libraries(rotor PUBLIC ${Boost_LIBRARIES})

add_executable(rotor-ping_pong src/rotor-ping_pong.cpp)
target_include_directories(rotor-ping_pong PUBLIC ${ROTOR_INCLUDE_DIRS})
target_link_libraries(rotor-ping_pong PUBLIC ${Boost_LIBRARIES})

add_executable(ping_pong_asio src/ping_pong_asio.cpp)
target_include_directories(ping_pong_asio PUBLIC ${ROTOR_INCLUDE_DIRS})
target_link_libraries(ping_pong_asio PUBLIC ${Boost_LIBRARIES})

add_executable(forwarder_sample src/forwarder_sample.cpp)
target_include_directories(forwarder_sample PUBLIC ${ROTOR_INCLUDE_DIRS})
target_link_libraries(forwarder_sample PUBLIC ${Boost_LIBRARIES})

