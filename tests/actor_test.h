//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#pragma once

#include "rotor/actor_base.h"
#include <list>
#include <functional>

namespace rotor {
namespace test {

namespace payload {

struct sample_t {
    int value;
};

} // namespace payload

namespace message {
using sample_t = rotor::message_t<payload::sample_t>;
}

struct actor_test_t;
using plugin_configurer_t = std::function<void(actor_base_t &self, plugin::plugin_base_t &)>;
using shutdown_fn_t = std::function<void(actor_test_t &self)>;

struct actor_test_t : public actor_base_t {
    using actor_base_t::actor_base_t;

    ~actor_test_t();
    plugin_configurer_t configurer;
    shutdown_fn_t shutdowner;

    void configure(plugin::plugin_base_t &plugin) noexcept override;
    auto &get_plugins() const noexcept { return plugins; }
    auto get_activating_plugins() noexcept { return this->activating_plugins; }
    auto get_deactivating_plugins() noexcept { return this->deactivating_plugins; }

    auto &get_state() noexcept { return state; }

    void shutdown_finish() noexcept override;
    void force_cleanup() noexcept;
};

} // namespace test
} // namespace rotor
