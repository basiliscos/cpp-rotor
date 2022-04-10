cmake_minimum_required(VERSION 3.8)
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.12)
    cmake_policy(SET CMP0074 NEW)
endif()

set(ROTOR_VERSION "0.22")
project (rotor LANGUAGES CXX VERSION ${ROTOR_VERSION})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

if (NOT DEFINED CMAKE_CXX_VISIBILITY_PRESET AND
    NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
endif ()

include(CMakePrintHelpers)

option(BUILD_BOOST_ASIO     "Enable building with boost::asio support [default: OFF]"    OFF)
option(BUILD_WX             "Enable building with wxWidgets support   [default: OFF]"    OFF)
option(BUILD_EV             "Enable building with libev support   [default: OFF]"        OFF)
option(BUILD_THREAD         "Enable building with thread support  [default: ON]"          ON)
option(BUILD_EXAMPLES       "Enable building examples [default: OFF]"                    OFF)
option(BUILD_DOC            "Enable building documentation [default: OFF]"               OFF)
option(BUILD_THREAD_UNSAFE  "Enable building thead-unsafe library [default: OFF]"        OFF)
option(ROTOR_DEBUG_DELIVERY "Enable runtime messages debuging [default: OFF]"            OFF)


find_package(Boost COMPONENTS
        date_time
        system
        regex
    REQUIRED)

include(GenerateExportHeader)

if (DEFINED rotor_SHARED_LIBS)
    set(BUILD_SHARED_LIBS "${rotor_SHARED_LIBS}")
endif ()


set(ROTOR_TARGETS_TO_INSTALL)
set(ROTOR_HEADERS_TO_INSTALL)

add_library(rotor
    src/rotor/actor_base.cpp
    src/rotor/address_mapping.cpp
    src/rotor/error_code.cpp
    src/rotor/extended_error.cpp
    src/rotor/handler.cpp
    src/rotor/message.cpp
    src/rotor/registry.cpp
    src/rotor/spawner.cpp
    src/rotor/subscription.cpp
    src/rotor/subscription_point.cpp
    src/rotor/supervisor.cpp
    src/rotor/system_context.cpp
    src/rotor/detail/child_info.cpp
    src/rotor/plugin/address_maker.cpp
    src/rotor/plugin/child_manager.cpp
    src/rotor/plugin/delivery.cpp
    src/rotor/plugin/foreigners_support.cpp
    src/rotor/plugin/init_shutdown.cpp
    src/rotor/plugin/lifetime.cpp
    src/rotor/plugin/link_client.cpp
    src/rotor/plugin/link_server.cpp
    src/rotor/plugin/locality.cpp
    src/rotor/plugin/plugin_base.cpp
    src/rotor/plugin/registry.cpp
    src/rotor/plugin/resources.cpp
    src/rotor/plugin/starter.cpp
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

if (BUILD_THREAD_UNSAFE)
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
list(APPEND ROTOR_HEADERS_TO_INSTALL
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
    include/rotor/messages.hpp
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

if (BUILD_BOOST_ASIO)
    find_package(Threads)
    add_library(rotor_asio
        src/rotor/asio/supervisor_asio.cpp
    )
    generate_export_header(rotor_asio
        EXPORT_MACRO_NAME ROTOR_ASIO_API
        EXPORT_FILE_NAME include/rotor/asio/export.h
    )
    if (WIN32)
        list(APPEND ROTOR_ASIO_PRIVATE_FLAGS
            _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
            _WIN32_WINNT=0x600
        )
    endif()
    target_compile_definitions(rotor_asio PRIVATE ${ROTOR_ASIO_PRIVATE_FLAGS})
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
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/rotor/asio/export.h
        DESTINATION include/rotor)
endif()

if (BUILD_WX)
    find_package(wxWidgets COMPONENTS base REQUIRED)
    include(${wxWidgets_USE_FILE})
    add_library(rotor_wx
        src/rotor/wx/supervisor_wx.cpp
        src/rotor/wx/system_context_wx.cpp
    )
    generate_export_header(rotor_wx
        EXPORT_MACRO_NAME ROTOR_WX_API
        EXPORT_FILE_NAME include/rotor/wx/export.h
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
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/rotor/wx/export.h
        DESTINATION include/rotor)
endif()

if (BUILD_EV)
    find_package(Libev REQUIRED)
    add_library(rotor_ev
        src/rotor/ev/supervisor_ev.cpp
        src/rotor/ev/system_context_ev.cpp
    )
    generate_export_header(rotor_ev
        EXPORT_MACRO_NAME ROTOR_EV_API
        EXPORT_FILE_NAME include/rotor/ev/export.h
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
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/rotor/ev/export.h
        DESTINATION include/rotor)
endif()

if (BUILD_THREAD)
    find_package(Threads REQUIRED)
    add_library(rotor_thread
        src/rotor/thread/supervisor_thread.cpp
        src/rotor/thread/system_context_thread.cpp
    )
    generate_export_header(rotor_thread
        EXPORT_MACRO_NAME ROTOR_THREAD_API
        EXPORT_FILE_NAME include/rotor/thread/export.h
    )
    target_link_libraries(rotor_thread PUBLIC rotor Threads::Threads)
    add_library(rotor::thread ALIAS rotor_thread)
    list(APPEND ROTOR_TARGETS_TO_INSTALL rotor_thread)
    list(APPEND ROTOR_HEADERS_TO_INSTALL
        include/rotor/thread.hpp
        include/rotor/thread/supervisor_thread.h
        include/rotor/thread/system_context_thread.h
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/rotor/thread/export.h
        DESTINATION include/rotor)
endif()

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if (BUILD_TESTING)
        add_subdirectory("tests")
    endif()
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
    install( FILES ${HEADER_FILE} DESTINATION ${DIR} )
endforeach()

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/include/rotor/export.h
    DESTINATION include/rotor)

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

include(CMakePackageConfigHelpers)
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
