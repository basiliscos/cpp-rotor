cmake_minimum_required(VERSION 3.23)
project(test_package CXX)

find_package("rotor" COMPONENTS asio thread REQUIRED)

if (WIN32)
    add_compile_definitions(
        _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
        _WIN32_WINNT=0x600
    )
endif()

add_executable(${PROJECT_NAME} test_package.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PROJECT_NAME} rotor::asio rotor::thread)
