add_library(test_lib test-main.cpp)

include_directories(${rotor_SOURCE_DIR}/src)
set(rotor_INCLUDE_DIRS ${rotor_SOURCE_DIR}/src ${rotor_SOURCE_DIR}/include ${rotor_SOURCE_DIR}/tests)

find_package(
    Boost
    COMPONENTS
        program_options
        log
        locale
        system
        filesystem
    REQUIRED
)

set(rotor_TEST_LIBS test_lib ${Boost_LIBRARIES})

add_executable(010-sup-start_stop 010-sup-start_stop.cpp)
target_link_libraries(010-sup-start_stop ${rotor_TEST_LIBS})
target_include_directories(010-sup-start_stop PUBLIC ${rotor_INCLUDE_DIRS})
add_test(010-sup-start_stop "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/010-sup-start_stop")
