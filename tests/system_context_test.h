#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/system_context.h"

namespace rotor {
namespace test {

struct system_context_test_t : public rotor::system_context_t {
    std::error_code ec;

    system_context_test_t() {

    }

    virtual void on_error(const std::error_code &ec_) noexcept override {
        ec = ec_;
    }

};

}
}
