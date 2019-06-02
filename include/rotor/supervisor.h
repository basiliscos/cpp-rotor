#pragma once

#include "actor_base.h"
#include "handler.hpp"
#include "message.h"
#include "messages.hpp"
#include "subscription.h"
#include "system_context.h"
#include <chrono>
#include <deque>
#include <functional>
#include <unordered_map>
#include <unordered_set>
//#include <iostream>

namespace rotor {

struct supervisor_t : public actor_base_t {

    supervisor_t(supervisor_t *sup = nullptr);
    supervisor_t(const supervisor_t &) = delete;
    supervisor_t(supervisor_t &&) = delete;

    inline void set_context(system_context_t *ctx) { context = ctx; }

    virtual void do_initialize() noexcept override;
    virtual void do_start() noexcept;
    virtual void do_process() noexcept;

    virtual void proccess_subscriptions() noexcept;
    virtual void proccess_unsubscriptions() noexcept;
    virtual void unsubscribe_actor(address_ptr_t addr, handler_ptr_t &handler_ptr) noexcept;
    virtual void unsubscribe_actor(const actor_ptr_t &actor, bool remove_actor = true) noexcept;
    virtual address_ptr_t make_address() noexcept;

    virtual void on_create(message_t<payload::create_actor_t> &msg) noexcept;
    virtual void on_initialize(message_t<payload::initialize_actor_t> &msg) noexcept override;
    virtual void on_initialize_confirm(message_t<payload::initialize_confirmation_t> &msg) noexcept;

    virtual void on_shutdown(message_t<payload::shutdown_request_t> &) noexcept override;
    virtual void on_shutdown_confirm(message_t<payload::shutdown_confirmation_t> &message) noexcept;

    virtual void on_shutdown_timer_trigger() noexcept;
    virtual void start_shutdown_timer() noexcept = 0;
    virtual void cancel_shutdown_timer() noexcept = 0;
    virtual void start() noexcept = 0;
    virtual void shutdown() noexcept = 0;
    virtual void enqueue(message_ptr_t message) noexcept = 0;

    enum class state_t {
        NEW,
        INITIALIZED,
        OPERATIONAL,
        SHUTTING_DOWN,
        SHUTTED_DOWN,
    };

    using subscription_queue_t = std::deque<subscription_request_t>;
    using unsubscription_queue_t = std::deque<subscription_request_t>;
    using queue_t = std::deque<message_ptr_t>;
    using subscription_map_t = std::unordered_map<address_ptr_t, subscription_t>;
    using actors_map_t = std::unordered_map<address_ptr_t, actor_ptr_t>;

    system_context_t *context;
    queue_t outbound;
    subscription_map_t subscription_map;
    actors_map_t actors_map;
    state_t state;
    unsubscription_queue_t unsubscription_queue;
    subscription_queue_t subscription_queue;

    inline void put(message_ptr_t message) { outbound.emplace_back(std::move(message)); }

    template <typename Handler> void subscribe_actor(actor_base_t &actor, Handler &&handler) {
        subscribe_actor(actor.get_address(), actor, std::move(handler));
    }

    template <typename Handler> void subscribe_actor(address_ptr_t addr, actor_base_t &actor, Handler &&handler) {
        using final_handler_t = handler_t<Handler>;
        auto handler_raw = new final_handler_t(actor, std::move(handler));
        auto handler_ptr = handler_ptr_t{handler_raw};

        subscription_queue.emplace_back(subscription_request_t{std::move(handler_ptr), std::move(addr)});
    }

    template <typename Actor, typename... Args> intrusive_ptr_t<Actor> create_actor(Args... args) {
        using wrapper_t = intrusive_ptr_t<Actor>;
        if (state != state_t::INITIALIZED)
            context->on_error(make_error_code(error_code_t::supervisor_wrong_state));
        auto raw_object = new Actor{*this, std::forward<Args>(args)...};
        raw_object->do_initialize();
        auto wrapper = wrapper_t{raw_object};
        send<payload::create_actor_t>(address, wrapper);
        return wrapper;
    }
};

using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

/* third-party classes implementations */

template <typename M, typename... Args> auto make_message(address_ptr_t addr, Args &&... args) -> message_ptr_t {
    return message_ptr_t{new message_t<M>(std::move(addr), std::forward<Args>(args)...)};
};

template <typename Supervisor, typename... Args>
auto system_context_t::create_supervisor(Args... args) -> intrusive_ptr_t<Supervisor> {
    using wrapper_t = intrusive_ptr_t<Supervisor>;
    auto raw_object = new Supervisor{std::forward<Args>(args)...};
    raw_object->set_context(this);
    supervisor = supervisor_ptr_t{raw_object};
    supervisor->do_initialize();
    return wrapper_t{raw_object};
}

template <typename M, typename... Args> void actor_base_t::send(const address_ptr_t &addr, Args &&... args) {
    supervisor.put(make_message<M>(addr, std::forward<Args>(args)...));
    // supervisor.enqueue<M>(addr, std::forward<Args>(args)...);
}

template <typename Handler> void actor_base_t::subscribe(Handler &&h) {
    supervisor.subscribe_actor(address, *this, std::forward<Handler>(h));
}

template <typename Handler> void actor_base_t::subscribe(Handler &&h, address_ptr_t &addr) {
    supervisor.subscribe_actor(addr, *this, std::forward<Handler>(h));
}

} // namespace rotor
