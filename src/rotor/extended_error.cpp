//
// Copyright (c) 2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/extended_error.h"
#include <sstream>

namespace rotor {

std::string extended_error_t::message() const noexcept {
    std::stringstream out;
    out << context << " " << ec.message();
    if (next) {
        out << " <- " << next->message();
    }
    return out.str();
}

extended_error_ptr_t extended_error_t::root() const noexcept {
    if (!next) {
        auto self = const_cast<extended_error_t *>(this);
        return extended_error_ptr_t(self);
    }
    auto n = next;
    while (n->next) {
        n = n->next;
    }
    return n;
}

extended_error_ptr_t make_error(const std::string &context_, const std::error_code &ec_,
                                const extended_error_ptr_t &next_) noexcept {
    return new extended_error_t(context_, ec_, next_);
}

} // namespace rotor
