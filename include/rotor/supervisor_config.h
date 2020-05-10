#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <boost/date_time/posix_time/posix_time.hpp>
#include "policy.h"
#include "actor_config.h"

namespace rotor {

namespace pt = boost::posix_time;

/** \struct supervisor_config_t
 *  \brief base supervisor config, which holds shutdowm timeout value
 */
struct supervisor_config_t : actor_config_t {

    /** \brief how to behave if child-actor fails */
    supervisor_policy_t policy = supervisor_policy_t::shutdown_self;

    using actor_config_t::actor_config_t;
};

template <typename Supervisor> struct supervisor_config_builder_t : actor_config_builder_t<Supervisor> {
    using parent_t = actor_config_builder_t<Supervisor>;
    using parent_t::parent_t;

    using plugins_list_t = std::tuple<
        internal::locality_plugin_t,
        internal::actor_lifetime_plugin_t,
        internal::subscription_plugin_t,
        internal::init_shutdown_plugin_t,
        internal::initializer_plugin_t,
        internal::starter_plugin_t,
        internal::subscription_support_plugin_t,
        internal::children_manager_plugin_t
    >;

    supervisor_config_builder_t &&policy(supervisor_policy_t policy_) && {
        parent_t::config.policy = policy_;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }
};

} // namespace rotor
