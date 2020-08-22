//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/error_code.h"

namespace rotor {
namespace details {

const char *error_code_category::name() const noexcept { return "rotor_error"; }

std::string error_code_category::message(int c) const {
    switch (static_cast<error_code_t>(c)) {
    case error_code_t::success:
        return "success";
    case error_code_t::request_timeout:
        return "request timeout";
    case error_code_t::supervisor_defined:
        return "supervisor is already defined";
    case error_code_t::already_registered:
        return "service name is already registered";
    case error_code_t::actor_misconfigured:
        return "actor is misconfigured";
    case error_code_t::actor_not_linkable:
        return "actor is not linkeable";
    case error_code_t::already_linked:
        return "already linked";
    case error_code_t::unknown_service:
        return "the requested service name is not registered";
    }
    return "unknown";
}

} // namespace details
} // namespace rotor

namespace rotor {

const static details::error_code_category category;
const details::error_code_category &error_code_category() { return category; }

} // namespace rotor
