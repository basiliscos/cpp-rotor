#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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

/** \brief list of raw plugin pointers*/
using plugins_t = std::deque<plugin::plugin_base_t *>;

/** \struct  plugin_storage_base_t
 * \brief abstract item to store plugins inside actor */
struct plugin_storage_base_t {
    virtual ~plugin_storage_base_t() {}

    /** \brief returns list of plugins pointers from the storage */
    virtual plugins_t get_plugins() noexcept = 0;
};

/** \brief smaprt pointer for {@link plugin_storage_base_t} */
using plugin_storage_ptr_t = std::unique_ptr<plugin_storage_base_t>;

/** \brief templated plugin storage implementation */
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

/** \struct actor_config_t
 * \brief basic actor configuration: init and shutdown timeouts, etc.
 */
struct actor_config_t {
    /** \brief constructs {@link plugin_storage_ptr_t} (type) */
    using plugins_constructor_t = std::function<plugin_storage_ptr_t()>;

    /** \brief constructs {@link plugin_storage_ptr_t} */
    plugins_constructor_t plugins_constructor;

    /** \brief raw pointer to {@link supervisor_t} */
    supervisor_t *supervisor;

    /** \brief max actor initialization time */
    pt::time_duration init_timeout;

    /** \brief max actor shutdown time */
    pt::time_duration shutdown_timeout;

    address_ptr_t spawner_address;

    bool escalate_failure = false;

    bool autoshutdown_supervisor = false;

    /** \brief constructs actor_config_t from raw supervisor pointer */
    actor_config_t(supervisor_t *supervisor_) : supervisor{supervisor_} {}
};

/** \brief CRTP actor config builder */
template <typename Actor> struct actor_config_builder_t {
    /** \brief final builder class */
    using builder_t = typename Actor::template config_builder_t<Actor>;

    /** \brief final config class */
    using config_t = typename Actor::config_t;

    /** \brief intrusive pointer to actor */
    using actor_ptr_t = intrusive_ptr_t<Actor>;

    /** \brief actor post-consturctor callback type
     *
     * For example, supervisor on_init() is invoked early (right after instantiation)
     *
     */
    using install_action_t = std::function<void(actor_ptr_t &)>;

    /** \brief bit mask for init timeout validation */
    constexpr static const std::uint32_t INIT_TIMEOUT = 1 << 0;

    /** \brief bit mask for shutdown timeout validation */
    constexpr static const std::uint32_t SHUTDOWN_TIMEOUT = 1 << 1;

    /** \brief bit mask for all required fields */
    constexpr static const std::uint32_t requirements_mask = INIT_TIMEOUT | SHUTDOWN_TIMEOUT;

    /** \brief post-construction callback */
    install_action_t install_action;

    /** \brief raw pointer to `supervisor_t` (is `null` for top-level supervisors) */
    supervisor_t *supervisor;

    /** \brief refernce to `system_context_t` */
    system_context_t &system_context;

    /** \brief the currently build config */
    config_t config;

    /** \brief required fields mask (used for validation) */
    std::uint32_t mask = builder_t::requirements_mask;

    /** \brief ctor with install action and raw pointer to supervisor */
    actor_config_builder_t(install_action_t &&action_, supervisor_t *supervisor_);

    /** \brief ctor with install action and pointer to `system_context_t` */
    actor_config_builder_t(install_action_t &&action_, system_context_t &system_context_)
        : install_action{std::move(action_)}, supervisor{nullptr}, system_context{system_context_}, config{nullptr} {
        init_ctor();
    }

    /** \brief setter for init and shutdown timeout */
    builder_t &&timeout(const pt::time_duration &timeout) &&noexcept {
        config.init_timeout = config.shutdown_timeout = timeout;
        mask = (mask & (~INIT_TIMEOUT & ~SHUTDOWN_TIMEOUT));
        return std::move(*static_cast<builder_t *>(this));
    }

    /** \brief setter for init timeout */
    builder_t &&init_timeout(const pt::time_duration &timeout) &&noexcept {
        config.init_timeout = timeout;
        mask = (mask & ~INIT_TIMEOUT);
        return std::move(*static_cast<builder_t *>(this));
    }

    /** \brief setter for shutdown timeout */
    builder_t &&shutdown_timeout(const pt::time_duration &timeout) &&noexcept {
        config.shutdown_timeout = timeout;
        mask = (mask & ~SHUTDOWN_TIMEOUT);
        return std::move(*static_cast<builder_t *>(this));
    }

    builder_t &&spawner_address(const address_ptr_t &value) &&noexcept {
        config.spawner_address = value;
        return std::move(*static_cast<builder_t *>(this));
    }

    builder_t &&escalate_failure(bool value = true) &&noexcept {
        config.escalate_failure = value;
        return std::move(*static_cast<builder_t *>(this));
    }

    builder_t &&autoshutdown_supervisor(bool value = true) &&noexcept {
        config.autoshutdown_supervisor = value;
        return std::move(*static_cast<builder_t *>(this));
    }

    /** \brief checks whether config is valid, i.e. all necessary fields are set */
    virtual bool validate() noexcept { return mask ? false : true; }

    /** \brief constructs actor from the current config */
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
