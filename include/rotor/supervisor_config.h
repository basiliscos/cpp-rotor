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

    bool create_registry = false;

    address_ptr_t registry_address;
};

template <typename Supervisor> struct supervisor_config_builder_t : actor_config_builder_t<Supervisor> {
    using builder_t = typename Supervisor::template config_builder_t<Supervisor>;
    using parent_t = actor_config_builder_t<Supervisor>;
    using parent_t::parent_t;

    builder_t &&policy(supervisor_policy_t policy_) &&noexcept {
        parent_t::config.policy = policy_;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

    builder_t &&create_registry(bool value) &&noexcept {
        parent_t::config.create_registry = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

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
