cmake_minimum_required(VERSION 3.0)
project (rotor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ROTOR_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(BUILD_BOOST_ASIO    "Enable building with boost::asio support [default: OFF]"    OFF)
option(BUILD_WX            "Enable building with wxWidgets support   [default: OFF]"    OFF)
option(BUILD_EXAMPLES      "Enable building examples [default: OFF]"                    OFF)
option(BUILD_TESTS         "Enable building tests    [default: OFF]"                    OFF)
option(BUILD_DOC           "Enable building documentation [default: OFF]"               OFF)
option(BUILD_THREAD_UNSAFE "Enable building thead-unsafe library [default: OFF]"        OFF)


set(ROTOR_BOOST_COMPONENTS)
if (BUILD_BOOST_ASIO)
    set(ROTOR_BOOST_COMPONENTS date_time system regex)
endif()

find_package(Boost COMPONENTS ${ROTOR_BOOST_COMPONENTS} REQUIRED)

add_library(rotor
    src/rotor/actor_base.cpp
    src/rotor/error_code.cpp
    src/rotor/message.cpp
    src/rotor/subscription.cpp
    src/rotor/supervisor.cpp
    src/rotor/system_context.cpp
)
target_include_directories(rotor PUBLIC ${Boost_INCLUDE_DIRS} include/)
target_link_libraries(rotor PUBLIC ${Boost_LIBRARIES})
if (BUILD_THREAD_UNSAFE)
    target_compile_definitions(rotor PUBLIC "ROTOR_REFCOUNT_THREADUNSAFE")
endif()

add_library(rotor_core ALIAS rotor)

if (BUILD_BOOST_ASIO)
    find_package(Threads)
    add_library(rotor_asio
        src/rotor/asio/supervisor_asio.cpp
    )
    target_include_directories(rotor_asio PUBLIC ${Boost_INCLUDE_DIRS} include/)
    target_link_libraries(rotor_asio PUBLIC rotor Threads::Threads)
endif()

if (BUILD_WX)
    find_package(wxWidgets COMPONENTS base REQUIRED)
    include(${wxWidgets_USE_FILE})
    add_library(rotor_wx
        src/rotor/wx/supervisor_wx.cpp
        src/rotor/wx/system_context_wx.cpp
    )
    target_include_directories(rotor_wx PUBLIC include/)
    target_link_libraries(rotor_wx PUBLIC rotor ${wxWidgets_LIBRARIES})
endif()

if (BUILD_TESTS)
    add_subdirectory("tests")
endif()

if (BUILD_EXAMPLES)
    add_subdirectory("examples")
endif()

if(BUILD_DOC)
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
        if (CMAKE_BUILD_TYPE MATCHES "^[Rr]elease")
            set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
            set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
            configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
            add_custom_target( doc_doxygen ALL
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMENT "Generating API documentation with Doxygen"
                VERBATIM)
            endif()
            file(GLOB DOC_IMAGES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/docs/*.png)
            file(COPY ${DOC_IMAGES} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/doxygen)
    else()
        message("Doxygen need to be installed to generate the doxygen documentation")
    endif()
endif()
