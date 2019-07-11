#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "messages.hpp"
#include <unordered_map>
#include "state.h"
#include <list>

namespace rotor {

struct supervisor_t;
struct system_context_t;
struct handler_base_t;
using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

struct actor_base_t : public arc_base_t<actor_base_t> {
    struct subscription_request_t {
        handler_ptr_t handler;
        address_ptr_t address;
    };

    using subscription_points_t = std::list<subscription_request_t>;

    actor_base_t(supervisor_t &supervisor_);
    virtual ~actor_base_t();

    virtual void do_initialize(system_context_t *ctx) noexcept;
    virtual void do_shutdown() noexcept;
    virtual address_ptr_t create_address() noexcept;
    virtual void remove_subscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept;

    inline address_ptr_t get_address() const noexcept { return address; }
    inline supervisor_t &get_supervisor() const noexcept { return supervisor; }
    inline state_t &get_state() noexcept { return state; }
    inline subscription_points_t &get_subscription_points() noexcept { return points; }

    virtual void confirm_shutdown() noexcept;

    virtual void on_initialize(message_t<payload::initialize_actor_t> &) noexcept;
    virtual void on_start(message_t<payload::start_actor_t> &) noexcept;
    virtual void on_shutdown(message_t<payload::shutdown_request_t> &) noexcept;
    virtual void on_subscription(message_t<payload::subscription_confirmation_t> &) noexcept;
    virtual void on_unsubscription(message_t<payload::unsubscription_confirmation_t> &) noexcept;
    virtual void on_external_unsubscription(message_t<payload::external_unsubscription_t> &) noexcept;

    template <typename M, typename... Args> void send(const address_ptr_t &addr, Args &&... args);

    template <typename Handler> void subscribe(Handler &&h) noexcept;
    template <typename Handler> void subscribe(Handler &&h, address_ptr_t &addr) noexcept;
    template <typename Handler> void unsubscribe(Handler &&h) noexcept;
    template <typename Handler> void unsubscribe(Handler &&h, address_ptr_t &addr) noexcept;

  protected:
    friend struct supervisor_t;
    supervisor_t &supervisor;
    state_t state;
    address_ptr_t address;
    subscription_points_t points;
};

using actor_ptr_t = intrusive_ptr_t<actor_base_t>;

} // namespace rotor
