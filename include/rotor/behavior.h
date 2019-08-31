#pragma once
//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include <system_error>

namespace rotor {

struct actor_base_t;
struct supervisor_t;

enum class behavior_state_t {
    UNKNOWN,
    INIT_STARTED,
    INIT_ENDED,
    SHUTDOWN_STARTED,
    SHUTDOWN_CHILDREN_STARTED,
    SHUTDOWN_CHILDREN_FINISHED,
    UNSUBSCRIPTION_STARTED,
    UNSUBSCRIPTION_FINISHED,
    SHUTDOWN_ENDED,
};

struct actor_behavior_t {
    actor_behavior_t(actor_base_t &actor_) : actor{actor_}, substate{behavior_state_t::UNKNOWN} {}
    virtual ~actor_behavior_t();

    virtual void action_confirm_init() noexcept;
    virtual void action_finish_init() noexcept;

    virtual void action_unsubscribe_self() noexcept;
    virtual void action_confirm_shutdown() noexcept;
    virtual void action_commit_shutdown() noexcept;
    virtual void action_finish_shutdonw() noexcept;

    virtual void on_start_init() noexcept;
    virtual void on_start_shutdown() noexcept;
    virtual void on_unsubscription() noexcept;

    actor_base_t &actor;
    behavior_state_t substate;
};

struct supervisor_behavior_t : public actor_behavior_t {
    supervisor_behavior_t(supervisor_t &sup);

    virtual void action_shutdown_children() noexcept;
    virtual void on_start_shutdown() noexcept override;
    virtual void on_childen_removed() noexcept;
    virtual void on_shutdown_fail(const address_ptr_t &address, const std::error_code &ec) noexcept;
};

} // namespace rotor
