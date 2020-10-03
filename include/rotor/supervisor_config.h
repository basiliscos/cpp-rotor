#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "policy.h"
#include "actor_config.h"

namespace rotor {

/** \struct supervisor_config_t
 *  \brief base supervisor config, which holds shutdowm timeout value
 */
struct supervisor_config_t : actor_config_t {

    using actor_config_t::actor_config_t;

    /** \brief how to behave if child-actor fails */
    supervisor_policy_t policy = supervisor_policy_t::shutdown_self;

    /** \brief whether the registy actor should be instantiated by supervisor */
    bool create_registry = false;

    /** \brief whether supervisor should wait untill all actors confirmed
     * initialization, and only then send start signal to all of them */
    bool synchronize_start = false;

    /** \brief use the specified address of a registry
     *
     * Can be usesul if an registry was created on the different supervisors
     * hierarchy
     */
    address_ptr_t registry_address;
};

/** \brief CRTP supervisor config builder */
template <typename Supervisor> struct supervisor_config_builder_t : actor_config_builder_t<Supervisor> {
    /** \brief final builder class */
    using builder_t = typename Supervisor::template config_builder_t<Supervisor>;

    /** \brief parent config builder */
    using parent_t = actor_config_builder_t<Supervisor>;

    using parent_t::parent_t;

    /** \brief defines actor's startup policy */
    builder_t &&policy(supervisor_policy_t policy_) &&noexcept {
        parent_t::config.policy = policy_;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

    /** \brief instructs supervisor to create an registry */
    builder_t &&create_registry(bool value = true) &&noexcept {
        parent_t::config.create_registry = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

    /** \brief instructs supervisor to synchornize start on it's children actors */
    builder_t &&synchronize_start(bool value = true) &&noexcept {
        parent_t::config.synchronize_start = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

    /** \brief injects external registry address */
    builder_t &&registry_address(const address_ptr_t &value) &&noexcept {
        parent_t::config.registry_address = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

    virtual bool validate() noexcept {
        bool r = parent_t::validate();
        if (r) {
            r = !(parent_t::config.registry_address && parent_t::config.create_registry);
        }
        return r;
    }
};

} // namespace rotor
