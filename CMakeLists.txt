cmake_minimum_required(VERSION 3.15)

cmake_policy(SET CMP0074 NEW)

set(ROTOR_VERSION "0.34")
project (rotor LANGUAGES CXX VERSION ${ROTOR_VERSION})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

if (NOT DEFINED CMAKE_CXX_VISIBILITY_PRESET AND
    NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
endif ()

include(CMakePrintHelpers)
include(GNUInstallDirs)
include(GenerateExportHeader)
include(CMakePackageConfigHelpers)

option(ROTOR_BUILD_ASIO           "Enable building with boost::asio support [default: OFF]"    OFF)
option(ROTOR_BUILD_WX             "Enable building with wxWidgets support   [default: OFF]"    OFF)
option(ROTOR_BUILD_EV             "Enable building with libev support   [default: OFF]"        OFF)
option(ROTOR_BUILD_FLTK           "Enable building with fltk support   [default: OFF]"         OFF)
option(ROTOR_BUILD_THREAD         "Enable building with thread support  [default: ON]"         ON)
option(ROTOR_BUILD_EXAMPLES       "Enable building examples [default: OFF]"                    OFF)
option(ROTOR_BUILD_DOC            "Enable building documentation [default: OFF]"               OFF)
option(ROTOR_BUILD_TESTS          "Enable building tests library [default: OFF]"               OFF)
option(ROTOR_BUILD_THREAD_UNSAFE  "Enable building thread-unsafe library [default: OFF]"       OFF)
option(ROTOR_DEBUG_DELIVERY       "Enable runtime messages debugging [default: OFF]"           OFF)

# output binaries to bin/lin
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Boost COMPONENTS
        date_time
        system
        regex
    REQUIRED)

if (BUILD_SHARED_LIBS)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
endif ()

add_library(rotor)

file(GLOB CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/rotor/*.cpp")
file(GLOB DETAIL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/rotor/detail/*.cpp")
file(GLOB PLUGIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/rotor/plugin/*.cpp")
file(GLOB MISC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/rotor/misc/*.cpp")

target_sources(rotor PRIVATE
    ${CORE_SOURCES}
    ${DETAIL_SOURCES}
    ${PLUGIN_SOURCES}
    ${MISC_SOURCES}
)

generate_export_header(rotor
    EXPORT_MACRO_NAME ROTOR_API
    EXPORT_FILE_NAME include/rotor/export.h
)

target_include_directories(rotor
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(rotor PUBLIC Boost::date_time Boost::system Boost::regex)

if (ROTOR_BUILD_THREAD_UNSAFE)
    target_compile_definitions(rotor PUBLIC "ROTOR_REFCOUNT_THREADUNSAFE")
endif()
if (ROTOR_DEBUG_DELIVERY)
    list(APPEND ROTOR_PRIVATE_FLAGS ROTOR_DEBUG_DELIVERY)
endif()

if (WIN32)
    list(APPEND ROTOR_PRIVATE_FLAGS _CRT_SECURE_NO_WARNINGS)
endif()
target_compile_definitions(rotor
    PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:ROTOR_STATIC_DEFINE>"
    PRIVATE ${ROTOR_PRIVATE_FLAGS}
)

target_compile_features(rotor PUBLIC cxx_std_17)
set_target_properties(rotor PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
add_library(rotor::core ALIAS rotor)
list(APPEND ROTOR_TARGETS_TO_INSTALL rotor)

install(
    TARGETS rotor
    EXPORT ROTOR_ALL_TARGETS
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

set(ROTOR_HEADERS_TO_INSTALL
    include/rotor.hpp
    include/rotor/actor_base.h
    include/rotor/actor_config.h
    include/rotor/address.hpp
    include/rotor/address_mapping.h
    include/rotor/arc.hpp
    include/rotor/detail/child_info.h
    include/rotor/error_code.h
    include/rotor/extended_error.h
    include/rotor/forward.hpp
    include/rotor/handler.h
    include/rotor/message.h
    include/rotor/message_stringifier.h
    include/rotor/messages.hpp
    include/rotor/misc/default_stringifier.h
    include/rotor/plugin/address_maker.h
    include/rotor/plugin/child_manager.h
    include/rotor/plugin/delivery.h
    include/rotor/plugin/foreigners_support.h
    include/rotor/plugin/init_shutdown.h
    include/rotor/plugin/lifetime.h
    include/rotor/plugin/link_client.h
    include/rotor/plugin/link_server.h
    include/rotor/plugin/locality.h
    include/rotor/plugin/plugin_base.h
    include/rotor/plugin/registry.h
    include/rotor/plugin/resources.h
    include/rotor/plugin/starter.h
    include/rotor/plugins.h
    include/rotor/policy.h
    include/rotor/registry.h
    include/rotor/request.hpp
    include/rotor/spawner.h
    include/rotor/state.h
    include/rotor/subscription.h
    include/rotor/subscription_point.h
    include/rotor/supervisor.h
    include/rotor/supervisor_config.h
    include/rotor/system_context.h
    include/rotor/timer_handler.hpp
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/include/rotor/export.h
    DESTINATION include/rotor)

if (ROTOR_BUILD_ASIO)
    find_package(Threads)
    add_library(rotor_asio)

    file(GLOB ASIO_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/rotor/asio/*.cpp")
    file(GLOB ASIO_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/rotor/asio/*.h*")

    generate_export_header(rotor_asio
        EXPORT_MACRO_NAME ROTOR_ASIO_API
        EXPORT_FILE_NAME include/rotor/asio/export.h
    )

    list(APPEND ROTOR_HEADERS_TO_INSTALL
        include/rotor/asio.hpp
        include/rotor/asio/forwarder.hpp
        include/rotor/asio/supervisor_asio.h
        include/rotor/asio/supervisor_config_asio.h
        include/rotor/asio/system_context_asio.h
    )
    target_sources(rotor_asio PRIVATE src/rotor/asio/supervisor_asio.cpp)

    if (WIN32)
        list(APPEND ROTOR_ASIO_PRIVATE_FLAGS
            _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
            _WIN32_WINNT=0x600
        )
    endif()
    target_compile_definitions(rotor_asio PRIVATE ${ROTOR_ASIO_PRIVATE_FLAGS})
    target_link_libraries(rotor_asio
        PUBLIC
            rotor
            Threads::Threads
            $<$<PLATFORM_ID:Windows>:ws2_32>
    )
    add_library(rotor::asio ALIAS rotor_asio)

    install(
        TARGETS rotor_asio
        EXPORT ROTOR_ALL_TARGETS
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/rotor/asio/export.h
        DESTINATION include/rotor/asio)
endif()

if (ROTOR_BUILD_WX)
    find_package(wxWidgets COMPONENTS base REQUIRED)
    if (${wxWidgets_USE_FILE})
        include(${wxWidgets_USE_FILE})
    endif()
    add_library(rotor_wx)

    file(GLOB WX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/rotor/wx/*.cpp")
    file(GLOB WX_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/rotor/wx/*.h*")

    generate_export_header(rotor_wx
        EXPORT_MACRO_NAME ROTOR_WX_API
        EXPORT_FILE_NAME include/rotor/wx/export.h
    )

    list(APPEND ROTOR_HEADERS_TO_INSTALL
        include/rotor/wx.hpp
        include/rotor/wx/supervisor_config_wx.h
        include/rotor/wx/supervisor_wx.h
        include/rotor/wx/system_context_wx.h
    )

    target_sources(rotor_wx PRIVATE ${WX_SOURCES})

    target_link_libraries(rotor_wx PUBLIC rotor ${wxWidgets_LIBRARIES})
    add_library(rotor::wx ALIAS rotor_wx)

    install(
        TARGETS rotor_wx
        EXPORT ROTOR_ALL_TARGETS
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/rotor/wx/export.h
        DESTINATION include/rotor/wx)
endif()

if (ROTOR_BUILD_EV)
    find_package(Libev REQUIRED)
    add_library(rotor_ev)

    file(GLOB EV_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/rotor/ev/*.cpp")

    generate_export_header(rotor_ev
        EXPORT_MACRO_NAME ROTOR_EV_API
        EXPORT_FILE_NAME include/rotor/ev/export.h
    )

    list(APPEND ROTOR_HEADERS_TO_INSTALL
        include/rotor/ev.hpp
        include/rotor/ev/supervisor_config_ev.h
        include/rotor/ev/supervisor_ev.h
        include/rotor/ev/system_context_ev.h
    )

    target_sources(rotor_ev PRIVATE ${EV_SOURCES})

    target_include_directories(rotor_ev PUBLIC ${LIBEV_INCLUDE_DIRS})
    target_link_libraries(rotor_ev PUBLIC rotor libev::libev)
    add_library(rotor::ev ALIAS rotor_ev)

    install(
        TARGETS rotor_ev
        EXPORT ROTOR_ALL_TARGETS
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/rotor/ev/export.h
        DESTINATION include/rotor/ev)
endif()

if (ROTOR_BUILD_FLTK)
    find_package(fltk REQUIRED)
    add_library(rotor_fltk)

    file(GLOB FLTK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/rotor/fltk/*.cpp")

    generate_export_header(rotor_fltk
        EXPORT_MACRO_NAME ROTOR_FLTK_API
        EXPORT_FILE_NAME include/rotor/fltk/export.h
    )

    list(APPEND ROTOR_HEADERS_TO_INSTALL
        include/rotor/fltk.hpp
        include/rotor/fltk/supervisor_config_fltk.h
        include/rotor/fltk/supervisor_fltk.h
        include/rotor/fltk/system_context_fltk.h
    )

    target_sources(rotor_fltk PRIVATE ${FLTK_SOURCES})

    target_link_libraries(rotor_fltk PUBLIC rotor fltk::fltk)
    add_library(rotor::fltk ALIAS rotor_fltk)

    install(
        TARGETS rotor_fltk
        EXPORT ROTOR_ALL_TARGETS
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/rotor/fltk/export.h
        DESTINATION include/rotor/fltk)
endif()

if (ROTOR_BUILD_THREAD)
    find_package(Threads REQUIRED)

    add_library(rotor_thread)

    file(GLOB THREAD_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/rotor/thread/*.cpp")

    generate_export_header(rotor_thread
        EXPORT_MACRO_NAME ROTOR_THREAD_API
        EXPORT_FILE_NAME include/rotor/thread/export.h
    )

    list(APPEND ROTOR_HEADERS_TO_INSTALL
        include/rotor/thread.hpp
        include/rotor/thread/supervisor_thread.h
        include/rotor/thread/system_context_thread.h
        include/rotor/timer_handler.hpp
    )
    target_sources(rotor_thread PRIVATE ${THREAD_SOURCES})
    target_link_libraries(rotor_thread PUBLIC rotor Threads::Threads)
    add_library(rotor::thread ALIAS rotor_thread)

    install(
        TARGETS rotor_thread
        EXPORT ROTOR_ALL_TARGETS
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/rotor/thread/export.h
        DESTINATION include/rotor/thread)
endif()


if (ROTOR_BUILD_TESTS)
    enable_testing()
    add_subdirectory("tests")
endif()

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND ROTOR_BUILD_EXAMPLES)
    add_subdirectory("examples")
endif()

if(ROTOR_BUILD_DOC)
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

if (BUILD_SHARED_LIBS)
    set(type shared)
    message(STATUS "going to build shared library/libraries")
else ()
    set(type static)
endif ()

install(
    EXPORT ROTOR_ALL_TARGETS
    FILE rotor-${type}-targets.cmake
    NAMESPACE rotor::
    DESTINATION ${ROTOR_CMAKE_FILES_DEST}
)

set(ROTOR_CONFIG_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/rotor-config-version.cmake")
set(ROTOR_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/rotor-config.cmake")

write_basic_package_version_file(
    ${ROTOR_CONFIG_VERSION_FILE}
    VERSION ${ROTOR_VERSION}
    COMPATIBILITY ExactVersion
)
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/rotor-config.cmake.in"
    ${ROTOR_CONFIG_FILE}
    INSTALL_DESTINATION ${ROTOR_CMAKE_FILES_DEST}
    PATH_VARS ROTOR_VERSION
)

install(
    FILES ${ROTOR_CONFIG_FILE} ${ROTOR_CONFIG_VERSION_FILE}
    DESTINATION ${ROTOR_CMAKE_FILES_DEST}
)

foreach( HEADER_FILE ${ROTOR_HEADERS_TO_INSTALL} )
    get_filename_component( DIR ${HEADER_FILE} PATH )
    install( FILES ${HEADER_FILE} DESTINATION ${DIR} )
endforeach()
