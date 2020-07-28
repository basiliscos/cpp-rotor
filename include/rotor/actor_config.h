#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <deque>
#include <tuple>
#include <functional>
#include <memory>
#include "arc.hpp"
#include "plugins.h"
#include "policy.h"
#include "forward.hpp"

namespace rotor {

using plugins_t = std::deque<plugin::plugin_base_t *>;

struct plugin_storage_base_t {
    virtual ~plugin_storage_base_t() {}
    virtual plugins_t get_plugins() noexcept = 0;
};

using plugin_storage_ptr_t = std::unique_ptr<plugin_storage_base_t>;

template <typename PluginList> struct plugin_storage_t : plugin_storage_base_t {
    plugins_t get_plugins() noexcept override {
        plugins_t plugins;
        add_plugin(plugins);
        return plugins;
    }

  private:
    PluginList plugin_list;

    template <size_t Index = 0> void add_plugin(plugins_t &plugins) noexcept {
        plugins.emplace_back(&std::get<Index>(plugin_list));
        if constexpr (Index + 1 < std::tuple_size_v<PluginList>) {
            add_plugin<Index + 1>(plugins);
        }
    }
};

struct actor_config_t {
    using plugins_constructor_t = std::function<plugin_storage_ptr_t()>;

    plugins_constructor_t plugins_constructor;
    supervisor_t *supervisor;

    pt::time_duration init_timeout;

    /** \brief how much time is allowed to spend in shutdown for children actor */
    pt::time_duration shutdown_timeout;

    actor_config_t(supervisor_t *supervisor_) : supervisor{supervisor_} {}
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
        : install_action{std::move(action_)}, supervisor{nullptr}, system_context{system_context_}, config{nullptr} {
        init_ctor();
    }

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

    virtual bool validate() noexcept { return mask ? false : true; }
    actor_ptr_t finish() &&;

  private:
    void init_ctor() noexcept {
        config.plugins_constructor = []() -> plugin_storage_ptr_t {
            using plugins_list_t = typename Actor::plugins_list_t;
            using storage_t = plugin_storage_t<plugins_list_t>;
            return std::make_unique<storage_t>();
        };
    }
};

} // namespace rotor
