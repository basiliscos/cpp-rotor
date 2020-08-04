#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "../subscription.h"
#include "rotor/messages.hpp"

namespace rotor::plugin {

enum class config_phase_t { PREINIT = 0b01, INITIALIZING = 0b10 };

struct plugin_base_t {
    enum processing_result_t { CONSUMED = 0, IGNORED, FINISHED };

    enum reaction_t {
        INIT = 1 << 0,
        SHUTDOWN = 1 << 1,
        SUBSCRIPTION = 1 << 2,
        START = 1 << 3,
    };

    plugin_base_t() = default;
    plugin_base_t(const plugin_base_t &) = delete;
    virtual ~plugin_base_t();

    virtual const void *identity() const noexcept = 0;

    virtual void activate(actor_base_t *actor) noexcept;
    virtual void deactivate() noexcept;

    virtual bool handle_init(message::init_request_t *message) noexcept;
    virtual bool handle_shutdown(message::shutdown_request_t *message) noexcept;
    virtual bool handle_start(message::start_trigger_t *message) noexcept;

    virtual bool handle_unsubscription(const subscription_point_t &point, bool external) noexcept;
    virtual bool forget_subscription(const subscription_point_t &point) noexcept;
    virtual void forget_subscription(const subscription_info_ptr_t &info) noexcept;

    virtual processing_result_t handle_subscription(message::subscription_t &message) noexcept;

    template <typename Plugin, typename Fn>
    void with_casted(Fn &&fn, config_phase_t desired_phase = config_phase_t::INITIALIZING) noexcept {
        int phase_match = static_cast<int>(phase) & static_cast<int>(desired_phase);
        if (phase_match && identity() == Plugin::class_identity) {
            auto &final = static_cast<Plugin &>(*this);
            fn(final);
        }
    }

    std::size_t get_reaction() const noexcept { return reaction; }
    void reaction_on(reaction_t value) noexcept { reaction = reaction | value; }
    void reaction_off(reaction_t value) noexcept { reaction = reaction & ~value; }

    template <typename T> auto &access() noexcept;

  protected:
    template <typename Handler>
    subscription_info_ptr_t subscribe(Handler &&handler, const address_ptr_t &address) noexcept;
    template <typename Handler> subscription_info_ptr_t subscribe(Handler &&handler) noexcept;

    actor_base_t *actor;

  private:
    subscription_container_t own_subscriptions;
    std::size_t reaction = 0;
    config_phase_t phase = config_phase_t::PREINIT;
};

} // namespace rotor::plugin
