cmake_minimum_required(VERSION 3.8)
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.12)
    cmake_policy(SET CMP0074 NEW)
endif()

set(ROTOR_VERSION "0.07")
project (rotor LANGUAGES CXX VERSION ${ROTOR_VERSION})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

include(CMakePrintHelpers)

option(BUILD_BOOST_ASIO    "Enable building with boost::asio support [default: OFF]"    OFF)
option(BUILD_WX            "Enable building with wxWidgets support   [default: OFF]"    OFF)
option(BUILD_EV            "Enable building with libev support   [default: OFF]"        OFF)
option(BUILD_EXAMPLES      "Enable building examples [default: OFF]"                    OFF)
option(BUILD_TESTS         "Enable building tests    [default: OFF]"                    OFF)
option(BUILD_DOC           "Enable building documentation [default: OFF]"               OFF)
option(BUILD_THREAD_UNSAFE "Enable building thead-unsafe library [default: OFF]"        OFF)


set(ROTOR_BOOST_COMPONENTS)
if (BUILD_BOOST_ASIO)
    set(ROTOR_BOOST_COMPONENTS date_time system regex)
endif()

find_package(Boost COMPONENTS ${ROTOR_BOOST_COMPONENTS} REQUIRED)
set(ROTOR_TARGETS_TO_INSTALL)
set(ROTOR_HEADERS_TO_INSTALL)

add_library(rotor
    src/rotor/actor_base.cpp
    src/rotor/address_mapping.cpp
    src/rotor/behavior.cpp
    src/rotor/error_code.cpp
    src/rotor/registry.cpp
    src/rotor/subscription.cpp
    src/rotor/supervisor.cpp
    src/rotor/system_context.cpp
)
target_include_directories(rotor
    PUBLIC
        ${Boost_INCLUDE_DIRS}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(rotor PUBLIC ${Boost_LIBRARIES})
if (BUILD_THREAD_UNSAFE)
    target_compile_definitions(rotor PUBLIC "ROTOR_REFCOUNT_THREADUNSAFE")
endif()
target_compile_features(rotor PUBLIC cxx_std_17)
set_target_properties(rotor PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
add_library(rotor::core ALIAS rotor)
list(APPEND ROTOR_TARGETS_TO_INSTALL rotor)
list(APPEND ROTOR_HEADERS_TO_INSTALL
    include/rotor.hpp
    include/rotor/actor_base.h
    include/rotor/address.hpp
    include/rotor/address_mapping.h
    include/rotor/arc.hpp
    include/rotor/behavior.h
    include/rotor/error_code.h
    include/rotor/handler.hpp
    include/rotor/message.h
    include/rotor/messages.hpp
    include/rotor/policy.h
    include/rotor/registry.h
    include/rotor/request.hpp
    include/rotor/state.h
    include/rotor/subscription.h
    include/rotor/supervisor.h
    include/rotor/supervisor_config.h
    include/rotor/system_context.h
)

if (BUILD_BOOST_ASIO)
    find_package(Threads)
    add_library(rotor_asio
        src/rotor/asio/supervisor_asio.cpp
    )
    target_include_directories(rotor_asio PUBLIC ${Boost_INCLUDE_DIRS})
    target_link_libraries(rotor_asio PUBLIC rotor Threads::Threads)
    add_library(rotor::asio ALIAS rotor_asio)
    list(APPEND ROTOR_TARGETS_TO_INSTALL rotor_asio)
    list(APPEND ROTOR_HEADERS_TO_INSTALL
        include/rotor/asio.hpp
        include/rotor/asio/forwarder.hpp
        include/rotor/asio/supervisor_asio.h
        include/rotor/asio/supervisor_config_asio.h
        include/rotor/asio/system_context_asio.h
    )
endif()

if (BUILD_WX)
    find_package(wxWidgets COMPONENTS base REQUIRED)
    include(${wxWidgets_USE_FILE})
    add_library(rotor_wx
        src/rotor/wx/supervisor_wx.cpp
        src/rotor/wx/system_context_wx.cpp
    )
    target_link_libraries(rotor_wx PUBLIC rotor ${wxWidgets_LIBRARIES})
    add_library(rotor::wx ALIAS rotor_wx)
    list(APPEND ROTOR_TARGETS_TO_INSTALL rotor_wx)
    list(APPEND ROTOR_HEADERS_TO_INSTALL
        include/rotor/wx.hpp
        include/rotor/wx/supervisor_config_wx.h
        include/rotor/wx/supervisor_wx.h
        include/rotor/wx/system_context_wx.h
    )
endif()

if (BUILD_EV)
    find_package(Libev REQUIRED)
    add_library(rotor_ev
        src/rotor/ev/supervisor_ev.cpp
        src/rotor/ev/system_context_ev.cpp
    )
    target_include_directories(rotor_ev PUBLIC ${LIBEV_INCLUDE_DIRS})
    target_link_libraries(rotor_ev PUBLIC rotor ${LIBEV_LIBRARY})
    add_library(rotor::ev ALIAS rotor_ev)
    list(APPEND ROTOR_TARGETS_TO_INSTALL rotor_ev)
    list(APPEND ROTOR_HEADERS_TO_INSTALL
        include/rotor/ev.hpp
        include/rotor/ev/supervisor_config_ev.h
        include/rotor/ev/supervisor_ev.h
        include/rotor/ev/system_context_ev.h
    )
endif()

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTS)
    enable_testing()
    add_subdirectory("tests")
endif()

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_EXAMPLES)
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


set(ROTOR_CMAKE_FILES_DEST "lib/cmake/rotor")
install(
    TARGETS ${ROTOR_TARGETS_TO_INSTALL}
    EXPORT ROTOR_ALL_TARGETS
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

foreach( HEADER_FILE ${ROTOR_HEADERS_TO_INSTALL} )
    get_filename_component( DIR ${HEADER_FILE} PATH )
    install( FILES ${HEADER_FILE} DESTINATION include/rotor/${DIR} )
endforeach()

install(
    EXPORT ROTOR_ALL_TARGETS
    FILE rotor-targets.cmake
    NAMESPACE rotor::
    DESTINATION ${ROTOR_CMAKE_FILES_DEST}
)

set(ROTOR_CONFIG_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/rotor-config-version.cmake")
set(ROTOR_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/rotor-config.cmake")

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${ROTOR_CONFIG_VERSION_FILE}
    VERSION ${ROTOR_VERSION}
    COMPATIBILITY ExactVersion
)
configure_package_config_file(
    "cmake/rotor-config.cmake.in"
    ${ROTOR_CONFIG_FILE}
    INSTALL_DESTINATION ${ROTOR_CMAKE_FILES_DEST}
    PATH_VARS ROTOR_VERSION
)

install(
    FILES ${ROTOR_CONFIG_FILE} ${ROTOR_CONFIG_VERSION_FILE}
    DESTINATION ${ROTOR_CMAKE_FILES_DEST}
)
