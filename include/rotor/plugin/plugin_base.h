#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "../subscription.h"
#include "rotor/messages.hpp"

namespace rotor::plugin {

enum class start_policy_t { early, late };

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

    template <typename Plugin, typename Fn> void with_casted(Fn &&fn) noexcept {
        if (identity() == Plugin::class_identity) {
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
};

template <start_policy_t Policy> struct plugin_t;

template <> struct plugin_t<start_policy_t::early> : plugin_base_t {
    void activate(actor_base_t *actor) noexcept override;
};

template <> struct plugin_t<start_policy_t::late> : plugin_base_t {
    void activate(actor_base_t *actor) noexcept override;
    bool handle_init(message::init_request_t *) noexcept override;

  protected:
    virtual void post_configure() noexcept = 0;
    virtual bool configured_handle_init(message::init_request_t *) noexcept = 0;
    bool configured = false;

  private:
};

} // namespace rotor::plugin
