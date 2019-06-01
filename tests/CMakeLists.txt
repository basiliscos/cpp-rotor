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

