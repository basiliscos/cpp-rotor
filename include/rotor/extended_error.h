#pragma once

//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"
#include <string>
#include <system_error>

namespace rotor {

struct extended_error_t;

using extended_error_ptr_t = intrusive_ptr_t<extended_error_t>;

struct extended_error_t : arc_base_t<extended_error_t> {
    std::string context;
    std::error_code ec;
    extended_error_ptr_t next;

    extended_error_t(const std::string &context_, const std::error_code &ec_,
                     const extended_error_ptr_t &next_ = {}) noexcept
        : context{context_}, ec{ec_}, next{next_} {}
    std::string message() const noexcept;
};

extended_error_ptr_t make_error(const std::string &context_, const std::error_code &ec_,
                                const extended_error_ptr_t &next_ = {}) noexcept;

} // namespace rotor
