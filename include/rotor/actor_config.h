#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <optional>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "arc.hpp"
#include "policy.h"

namespace rotor {

struct supervisor_t;
struct system_context_t;

namespace pt = boost::posix_time;

struct actor_config_t {
    supervisor_t *supervisor;

    pt::time_duration init_timeout;

    /** \brief how much time is allowed to spend in shutdown for children actor */
    pt::time_duration shutdown_timeout;

    std::optional<pt::time_duration> unlink_timeout;

    unlink_policy_t unlink_policy = unlink_policy_t::ignore;

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

    std::uint32_t mask = builder_t::requirements_mask;

    actor_config_builder_t(install_action_t &&action_, supervisor_t *supervisor_);
    actor_config_builder_t(install_action_t &&action_, system_context_t &system_context_)
        : install_action{std::move(action_)}, supervisor{nullptr}, system_context{system_context_}, config{nullptr} {}

    builder_t &&timeout(const pt::time_duration &timeout) && {
        config.init_timeout = config.shutdown_timeout = timeout;
        mask = (mask & (~INIT_TIMEOUT & ~SHUTDOWN_TIMEOUT));
        return std::move(*static_cast<builder_t *>(this));
    }

    actor_config_builder_t &&init_timeout(const pt::time_duration &timeout) && {
        config.init_timeout = timeout;
        mask = (mask & ~INIT_TIMEOUT);
        return std::move(*static_cast<builder_t *>(this));
    }

    actor_config_builder_t &&shutdown_timeout(const pt::time_duration &timeout) && {
        config.shutdown_timeout = timeout;
        mask = (mask & ~SHUTDOWN_TIMEOUT);
        return std::move(*static_cast<builder_t *>(this));
    }

    actor_config_builder_t &&unlink_timeout(const pt::time_duration &timeout) && {
        config.unlink_timeout = timeout;
        return std::move(*static_cast<builder_t *>(this));
    }

    actor_config_builder_t &&unlink_policy(const unlink_policy_t &policy) && {
        config.unlink_policy = policy;
        return std::move(*static_cast<builder_t *>(this));
    }

    virtual bool validate() noexcept {
        if (mask) {
            return false;
        }
        if (config.unlink_timeout) {
            if (*config.unlink_timeout > config.shutdown_timeout) {
                return false;
            }
        }
        return true;
    }

    actor_ptr_t finish() &&;
};

} // namespace rotor
