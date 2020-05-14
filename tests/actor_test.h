#pragma once

//
// Copyright (c) 2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/actor_base.h"
#include <list>

namespace rotor {
namespace test {

struct actor_test_t: public actor_base_t {
    using actor_base_t::actor_base_t;

    auto get_active_plugins() noexcept -> actor_config_t::plugins_t& { return this->active_plugins; }
    auto get_inactive_plugins() noexcept -> actor_config_t::plugins_t& { return this->inactive_plugins; }
};

}
}
