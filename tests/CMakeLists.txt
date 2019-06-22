enable_testing()

add_library(rotor_test_lib
    actor_test.cpp
    supervisor_test.cpp
    test-main.cpp
)
target_link_libraries(rotor_test_lib rotor_core)

set(rotor_TEST_LIBS rotor_core rotor_test_lib)

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

add_executable(015-actor-statuses 015-actor-statuses.cpp)
target_link_libraries(015-actor-statuses ${rotor_TEST_LIBS})
add_test(015-actor-statuses "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/015-actor-statuses")

add_executable(020-two-supervisors 020-two-supervisors.cpp)
target_link_libraries(020-two-supervisors ${rotor_TEST_LIBS})
add_test(020-two-supervisors "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/020-two-supervisors")

add_executable(021-ping_pong-external 021-ping_pong-external.cpp)
target_link_libraries(021-ping_pong-external ${rotor_TEST_LIBS})
add_test(021-ping_pong-external "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/021-ping_pong-external")


if (BUILD_BOOST_ASIO)
    set(rotor_BOOTS_TEST_LIBS rotor_test_lib rotor_asio)

    # boost::asio tests
    add_executable(101-asio_ping-pong-1-strand 101-asio_ping-pong-1-strand.cpp)
    target_link_libraries(101-asio_ping-pong-1-strand ${rotor_BOOTS_TEST_LIBS})
    add_test(101-asio_ping-pong-1-strand "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/101-asio_ping-pong-1-strand")

    add_executable(102-asio_ping-pong-2-strands 102-asio_ping-pong-2-strands.cpp)
    target_link_libraries(102-asio_ping-pong-2-strands ${rotor_BOOTS_TEST_LIBS})
    add_test(102-asio_ping-pong-2-strands "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/102-asio_ping-pong-2-strands")

    add_executable(103-asio_ping-pong-2-threads 103-asio_ping-pong-2-threads.cpp)
    target_link_libraries(103-asio_ping-pong-2-threads ${rotor_BOOTS_TEST_LIBS})
    add_test(103-asio_ping-pong-2-threads "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/103-asio_ping-pong-2-threads")
endif()


if (BUILD_WX)
    find_package(wxWidgets COMPONENTS base core REQUIRED)
    include(${wxWidgets_USE_FILE})

    add_executable(121-wx_ping_ping 121-wx_ping_ping.cpp)
    target_link_libraries(121-wx_ping_ping rotor_test_lib rotor_wx ${wxWidgets_LIBRARIES})
    add_test(121-wx_ping_ping "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/121-wx_ping_ping")
endif()
