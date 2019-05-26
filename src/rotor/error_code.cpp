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
  default:
    return "unknown";
  }
}

} // namespace detail
} // namespace rotor

namespace rotor {

const static detail::error_code_category category;
const detail::error_code_category &error_code_category() { return category; }

} // namespace rotor
