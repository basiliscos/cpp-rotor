cmake_minimum_required(VERSION 3.23)

if (BUILD_SHARED_LIBS)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin)
endif ()

find_package(Catch2 REQUIRED)

add_library(rotor_test_lib STATIC
    access.cpp
    actor_test.cpp
    supervisor_test.cpp
)

target_link_libraries(rotor_test_lib rotor::core Catch2::Catch2WithMain)
add_library(rotor::test ALIAS rotor_test_lib)
set(rotor_TEST_LIBS rotor::core rotor::test)

file(GLOB COMMON_TESTS_SOURCES "0*.cpp")

include(CTest)
include(Catch)

foreach(SOURCES ${COMMON_TESTS_SOURCES})
    get_filename_component(EXEC_NAME ${SOURCES} NAME_WE)
    add_executable(${EXEC_NAME})
    target_sources(${EXEC_NAME} PRIVATE ${SOURCES})
    target_link_libraries(${EXEC_NAME} ${rotor_TEST_LIBS})
    catch_discover_tests(${EXEC_NAME} TEST_PREFIX "${EXEC_NAME} \\")
endforeach(SOURCES)


if (BUILD_BOOST_ASIO)
    set(rotor_BOOTS_TEST_LIBS rotor::test rotor::asio)

    if (WIN32)
        add_compile_definitions(
            _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
            _WIN32_WINNT=0x600
        )
    endif()

    add_executable(101-asio_ping-pong-1-strand 101-asio_ping-pong-1-strand.cpp)
    target_link_libraries(101-asio_ping-pong-1-strand ${rotor_BOOTS_TEST_LIBS})
    catch_discover_tests(101-asio_ping-pong-1-strand TEST_PREFIX "101-asio_ping-pong-1-strand \\")

    add_executable(102-asio_ping-pong-2-strands 102-asio_ping-pong-2-strands.cpp)
    target_link_libraries(102-asio_ping-pong-2-strands ${rotor_BOOTS_TEST_LIBS})
    catch_discover_tests(102-asio_ping-pong-2-strands TEST_PREFIX "102-asio_ping-pong-2-strands \\")

    if (NOT BUILD_THREAD_UNSAFE)
        add_executable(103-asio_ping-pong-2-threads 103-asio_ping-pong-2-threads.cpp)
        target_link_libraries(103-asio_ping-pong-2-threads ${rotor_BOOTS_TEST_LIBS})
        catch_discover_tests(103-asio_ping-pong-2-threads TEST_PREFIX "103-asio_ping-pong-2-threads \\")
    endif()

    add_executable(104-asio_timer 104-asio_timer.cpp)
    target_link_libraries(104-asio_timer ${rotor_BOOTS_TEST_LIBS})
    catch_discover_tests(104-asio_timer TEST_PREFIX "104-asio_timer \\")
endif()


if (BUILD_WX)
    add_executable(121-wx_ping_ping 121-wx_ping_ping.cpp)
    target_link_libraries(121-wx_ping_ping rotor::test rotor::wx)
    catch_discover_tests(121-wx_ping_ping)

    add_executable(122-wx_timer 122-wx_timer.cpp)
    target_link_libraries(122-wx_timer rotor::test rotor::wx)
    catch_discover_tests(122-wx_timer)
endif()


if (BUILD_EV)
    add_executable(131-ev_ping-pong 131-ev_ping-pong.cpp)
    target_link_libraries(131-ev_ping-pong rotor::test rotor::ev)
    catch_discover_tests(131-ev_ping-pong TEST_PREFIX "131-ev_ping-pong \\")

    add_executable(132-ev_timer 132-ev_timer.cpp)
    target_link_libraries(132-ev_timer rotor::test rotor::ev)
    catch_discover_tests(132-ev_timer TEST_PREFIX "132-ev_timer \\")
endif()

if (BUILD_THREAD)
    add_executable(141-thread_ping-pong 141-thread_ping-pong.cpp)
    target_link_libraries(141-thread_ping-pong rotor::test rotor::thread)
    catch_discover_tests(141-thread_ping-pong TEST_PREFIX "141-thread_ping-pong \\")

    add_executable(142-thread_timer 142-thread_timer.cpp)
    target_link_libraries(142-thread_timer rotor::test rotor::thread)
    catch_discover_tests(142-thread_timer TEST_PREFIX "142-thread_timer \\")

    add_executable(143-thread-shutdown_flag 143-thread-shutdown_flag.cpp)
    target_link_libraries(143-thread-shutdown_flag rotor::test rotor::thread)
    catch_discover_tests(143-thread-shutdown_flag TEST_PREFIX "143-thread-shutdown_flag \\")
endif()

if (BUILD_FLTK)
    add_executable(151-fltk-ping-pong 151-fltk-ping-pong.cpp)
    target_link_libraries(151-fltk-ping-pong rotor::test rotor::fltk $<$<PLATFORM_ID:Linux>:jpeg png>)
    catch_discover_tests(151-fltk-ping-pong TEST_PREFIX "151-fltk-ping-pong \\")
endif()
