//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <iostream>

#include "rotor/supervisor.h"
#include "rotor/system_context.h"

using namespace rotor;

system_context_t::system_context_t() {}

system_context_t::~system_context_t() {}

void system_context_t::on_error(const std::error_code &ec) noexcept {
    std::cerr << "fatal error: " << ec.message() << "\n";
    std::terminate();
}
