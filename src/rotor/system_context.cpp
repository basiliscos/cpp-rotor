//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <iostream>
#include <sstream>
#include <ios>

#include "rotor/supervisor.h"
#include "rotor/system_context.h"

using namespace rotor;

system_context_t::system_context_t() {}

system_context_t::~system_context_t() {}

void system_context_t::on_error(const extended_error_ptr_t &ec) noexcept {
    std::cerr << "fatal error: " << ec->message() << "\n";
    std::terminate();
}

std::string system_context_t::identity() noexcept {
    std::stringstream out;
    out << std::hex << (const void *)this;
    return out.str();
}
