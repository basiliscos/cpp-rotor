#include "rotor/error_code.h"

namespace rotor {
namespace detail {

const char *error_code_category::name() const noexcept { return "rotor_error"; }

std::string error_code_category::message(int c) const {
    switch (static_cast<error_code_t>(c)) {
    case error_code_t::success:
        return "success";
    case error_code_t::shutdown_timeout:
        return "shutdown timeout";
    case error_code_t::missing_actor:
        return "missing actor";
    case error_code_t::supervisor_defined:
        return "supervisor is already defined";
    case error_code_t::supervisor_wrong_state:
        return "supervisor is in the wrong state";
    default:
        return "unknown";
    }
}

} // namespace detail
} // namespace rotor

namespace rotor {

const static details::error_code_category category;
const details::error_code_category &error_code_category() { return category; }

} // namespace rotor
