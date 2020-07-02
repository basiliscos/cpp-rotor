#pragma once

//
// Copyright (c) 2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/actor_base.h"
#include <list>
#include <functional>

namespace rotor {
namespace test {

struct actor_test_t;
using plugin_configurer_t = std::function<void(actor_test_t& self, plugin_t&)>;

struct actor_test_t: public actor_base_t {
    using actor_base_t::actor_base_t;

    ~actor_test_t();
    plugin_configurer_t configurer;

    void configure(plugin_t &plugin) noexcept override;
    plugin_t* get_plugin(const void* identity) const noexcept;
    static plugin_t* get_plugin(const actor_config_t::plugins_t&, const void* identity) noexcept;
    auto get_activating_plugins() noexcept { return this->activating_plugins; }
    auto get_deactivating_plugins() noexcept { return this->deactivating_plugins; }
};

}
}
