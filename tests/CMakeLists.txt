enable_testing()
add_library(test_lib test-main.cpp)
target_include_directories(test_lib INTERFACE include/)

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
