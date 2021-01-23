//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/extended_error.h"
#include <sstream>

using namespace rotor;

std::string extended_error_t::message() const noexcept {
    std::stringstream out;
    out << context << " " << ec.message();
    if (next) {
        out << " <- " << next->message();
    }
    return out.str();
}
