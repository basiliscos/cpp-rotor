#pragma once

//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/system_context.h"

namespace rotor {
namespace test {

struct system_context_test_t : public rotor::system_context_t {
    extended_error_ptr_t reason;

    system_context_test_t() {

    }

    inline virtual void on_error(actor_base_t*, const extended_error_ptr_t &ec_) noexcept override {
        reason = ec_;
    }

};

}
}
