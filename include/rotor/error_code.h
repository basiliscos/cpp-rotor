#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <string>
#include <system_error>

namespace rotor {

/** \brief fatal error codes in rotor */
enum class error_code_t {
    success = 0,
    request_timeout,
    shutdown_timeout,
    missing_actor,
    supervisor_defined,
    unknown_request,
};

namespace details {

/** \brief category support for `rotor` error codes */
class error_code_category : public std::error_category {
    virtual const char *name() const noexcept override;
    virtual std::string message(int c) const override;
};

} // namespace details

/** \brief returns error code category for `rotor` error codes */
const details::error_code_category &error_code_category();

/** \brief makes `std::error_code` from rotor error_code enumerations */
inline std::error_code make_error_code(error_code_t e) { return {static_cast<int>(e), error_code_category()}; }

} // namespace rotor

namespace std {
template <> struct is_error_code_enum<rotor::error_code_t> : std::true_type {};
} // namespace std
