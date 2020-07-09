#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <optional>
#include <deque>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <tuple>
#include <functional>
#include "arc.hpp"
#include "plugins.h"
#include "policy.h"

namespace rotor {

struct supervisor_t;
struct system_context_t;
struct plugin_t;
struct actor_base_t;

namespace pt = boost::posix_time;

struct actor_config_t {
    using plugins_t = std::deque<plugin_t *>;
    using callback_t = std::function<void(actor_base_t &)>;

    plugins_t plugins;
    supervisor_t *supervisor;

    pt::time_duration init_timeout;

    /** \brief how much time is allowed to spend in shutdown for children actor */
    pt::time_duration shutdown_timeout;

    actor_config_t(supervisor_t *supervisor_) : supervisor{supervisor_} {}
    ~actor_config_t() {
        for (auto it : plugins) {
            delete it;
        }
    }
};

template <typename Actor> struct actor_config_builder_t {
    using builder_t = typename Actor::template config_builder_t<Actor>;
    using config_t = typename Actor::config_t;
    using actor_ptr_t = intrusive_ptr_t<Actor>;
    using install_action_t = std::function<void(actor_ptr_t &)>;

    constexpr static const std::uint32_t INIT_TIMEOUT = 1 << 0;
    constexpr static const std::uint32_t SHUTDOWN_TIMEOUT = 1 << 1;
    constexpr static const std::uint32_t requirements_mask = INIT_TIMEOUT | SHUTDOWN_TIMEOUT;

    install_action_t install_action;
    supervisor_t *supervisor;
    system_context_t &system_context;
    config_t config;
    bool plugins_expanded = false;

    std::uint32_t mask = builder_t::requirements_mask;

    actor_config_builder_t(install_action_t &&action_, supervisor_t *supervisor_);
    actor_config_builder_t(install_action_t &&action_, system_context_t &system_context_)
        : install_action{std::move(action_)}, supervisor{nullptr}, system_context{system_context_}, config{nullptr} {}

    builder_t &&timeout(const pt::time_duration &timeout) &&noexcept {
        config.init_timeout = config.shutdown_timeout = timeout;
        mask = (mask & (~INIT_TIMEOUT & ~SHUTDOWN_TIMEOUT));
        return std::move(*static_cast<builder_t *>(this));
    }

    builder_t &&init_timeout(const pt::time_duration &timeout) &&noexcept {
        config.init_timeout = timeout;
        mask = (mask & ~INIT_TIMEOUT);
        return std::move(*static_cast<builder_t *>(this));
    }

    builder_t &&shutdown_timeout(const pt::time_duration &timeout) &&noexcept {
        config.shutdown_timeout = timeout;
        mask = (mask & ~SHUTDOWN_TIMEOUT);
        return std::move(*static_cast<builder_t *>(this));
    }

    virtual bool validate() noexcept {
        instantiate_plugins();
        if (mask) {
            return false;
        }
        return true;
    }
    actor_ptr_t finish() &&;

  private:
    void instantiate_plugins() noexcept {
        if (!plugins_expanded) {
            using plugins_t = typename Actor::plugins_list_t;
            add_plugin<plugins_t>();
            plugins_expanded = true;
        }
    }

    template <typename PluginList, std::size_t Index = 0> void add_plugin() noexcept {
        using Plugin = std::tuple_element_t<Index, PluginList>;
        instantiate_plugin<Plugin>();
        if constexpr (Index + 1 < std::tuple_size_v<PluginList>) {
            add_plugin<PluginList, Index + 1>();
        }
    }

    template <typename Plugin> void instantiate_plugin() noexcept { config.plugins.emplace_back(new Plugin()); }
};

} // namespace rotor
