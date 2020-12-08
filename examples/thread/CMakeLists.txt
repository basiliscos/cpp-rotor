find_package(OpenSSL COMPONENTS Crypto)

if (OPENSSL_FOUND)
    add_executable(sha512 sha512.cpp)
    target_link_libraries(sha512 rotor::thread OpenSSL::Crypto)
    add_test(sha512 "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sha512")
endif()
