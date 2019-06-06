enable_testing()

add_library(test_lib
    supervisor_test.cpp
    test-main.cpp
)
#target_include_directories(test_lib INTERFACE include/)
target_link_libraries(test_lib rotor)

find_package(
    Boost
    COMPONENTS
        date_time
        system
    REQUIRED
)
find_package(Threads)

set(rotor_TEST_LIBS rotor test_lib ${Boost_LIBRARIES} Threads::Threads)

add_executable(010-sup-start_stop 010-sup-start_stop.cpp)
target_link_libraries(010-sup-start_stop ${rotor_TEST_LIBS})
add_test(010-sup-start_stop "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/010-sup-start_stop")

add_executable(011-ping_pong 011-ping_pong.cpp)
target_link_libraries(011-ping_pong ${rotor_TEST_LIBS})
add_test(011-ping_pong "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/011-ping_pong")

add_executable(012-actor-lifetime 012-actor-lifetime.cpp)
target_link_libraries(012-actor-lifetime ${rotor_TEST_LIBS})
add_test(012-actor-lifetime "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/012-actor-lifetime")

add_executable(013-observer 013-observer.cpp)
target_link_libraries(013-observer ${rotor_TEST_LIBS})
add_test(013-observer "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/013-observer")

add_executable(014-lifetime-observer 014-lifetime-observer.cpp)
target_link_libraries(014-lifetime-observer ${rotor_TEST_LIBS})
add_test(014-lifetime-observer "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/014-lifetime-observer")

add_executable(020-two-supervisors 020-two-supervisors.cpp)
target_link_libraries(020-two-supervisors ${rotor_TEST_LIBS})
add_test(020-two-supervisors "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/020-two-supervisors")

add_executable(021-ping_pong-external 021-ping_pong-external.cpp)
target_link_libraries(021-ping_pong-external ${rotor_TEST_LIBS})
add_test(021-ping_pong-external "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/021-ping_pong-external")

