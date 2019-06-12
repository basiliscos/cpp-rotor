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

struct supervisor_t;

template <typename Actor, typename IsSupervisor = void> struct actor_ctor_t;

template <typename Actor> struct actor_ctor_t<Actor, std::enable_if_t<std::is_base_of_v<supervisor_t, Actor>>> {
    template <typename... Args>
    static auto construct(supervisor_t *sup, Args... args) noexcept -> intrusive_ptr_t<Actor> {
        return new Actor{sup, std::forward<Args>(args)...};
    }
};

template <typename Actor> struct actor_ctor_t<Actor, std::enable_if_t<!std::is_base_of_v<supervisor_t, Actor>>> {
    template <typename... Args>
    static auto construct(supervisor_t *sup, Args... args) noexcept -> intrusive_ptr_t<Actor> {
        return new Actor{*sup, std::forward<Args>(args)...};
    }
};

struct supervisor_t : public actor_base_t {

    supervisor_t(supervisor_t *sup = nullptr);
    supervisor_t(const supervisor_t &) = delete;
    supervisor_t(supervisor_t &&) = delete;

    virtual void do_initialize(system_context_t *ctx) noexcept override;
    virtual void do_start() noexcept;
    virtual void do_process() noexcept;

    virtual void proccess_subscriptions() noexcept;
    virtual void proccess_unsubscriptions() noexcept;
    virtual void unsubscribe_actor(address_ptr_t addr, handler_ptr_t &&handler_ptr) noexcept;
    virtual void unsubscribe_actor(const actor_ptr_t &actor, bool remove_actor = true) noexcept;
    virtual address_ptr_t make_address() noexcept;

    virtual void on_create(message_t<payload::create_actor_t> &msg) noexcept;
    virtual void on_initialize(message_t<payload::initialize_actor_t> &msg) noexcept override;
    virtual void on_initialize_confirm(message_t<payload::initialize_confirmation_t> &msg) noexcept;

    virtual void on_shutdown(message_t<payload::shutdown_request_t> &) noexcept override;
    virtual void on_shutdown_confirm(message_t<payload::shutdown_confirmation_t> &message) noexcept;

    virtual void on_subscription(message_t<payload::external_subscription_t> &message) noexcept;
    virtual void on_unsubscription(message_t<payload::external_unsubscription_t> &message) noexcept;
    virtual void on_call(message_t<payload::handler_call_t> &message) noexcept;
    virtual void on_state_request(message_t<payload::state_request_t> &message) noexcept;

    virtual void on_shutdown_timer_trigger() noexcept;
    virtual void start_shutdown_timer() noexcept = 0;
    virtual void cancel_shutdown_timer() noexcept = 0;
    virtual void start() noexcept = 0;
    virtual void shutdown() noexcept = 0;
    virtual void enqueue(message_ptr_t message) noexcept = 0;

    inline supervisor_t *get_parent_supevisor() noexcept { return parent; }

    using subscription_queue_t = std::deque<subscription_request_t>;
    using unsubscription_queue_t = std::deque<subscription_request_t>;
    using queue_t = std::deque<message_ptr_t>;
    using subscription_map_t = std::unordered_map<address_ptr_t, subscription_t>;
    using actors_map_t = std::unordered_map<address_ptr_t, actor_ptr_t>;

    state_t state;
    supervisor_t *parent;
    system_context_t *context;
    queue_t outbound;
    subscription_map_t subscription_map;
    actors_map_t actors_map;
    unsubscription_queue_t unsubscription_queue;
    subscription_queue_t subscription_queue;

    inline void put(message_ptr_t message) { outbound.emplace_back(std::move(message)); }
    // inline void put(const message_ptr_t &message) { outbound.push_back(message); }

    template <typename Handler> void subscribe_actor(actor_base_t &actor, Handler &&handler) {
        supervisor.subscribe_actor(actor.get_address(), wrap_handler(actor, std::move(handler)));
    }

    inline void subscribe_actor(address_ptr_t addr, handler_ptr_t &&handler) {
        if (&addr->supervisor == &supervisor) {
            subscription_queue.emplace_back(subscription_request_t{std::move(handler), std::move(addr)});
        } else {
            send<payload::external_subscription_t>(addr->supervisor.address, addr, std::move(handler));
        }
    }

    template <typename Actor, typename... Args> intrusive_ptr_t<Actor> create_actor(Args... args) {
        using ctor_t = actor_ctor_t<Actor>;
        if (state != state_t::INITIALIZED)
            context->on_error(make_error_code(error_code_t::supervisor_wrong_state));
        auto actor = ctor_t::construct(this, std::forward<Args>(args)...);
        actor->do_initialize(context);
        send<payload::create_actor_t>(address, actor);
        return actor;
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
    raw_object->do_initialize(this);
    supervisor = supervisor_ptr_t{raw_object};
    return wrapper_t{raw_object};
}

template <typename M, typename... Args> void actor_base_t::send(const address_ptr_t &addr, Args &&... args) {
    supervisor.put(make_message<M>(addr, std::forward<Args>(args)...));
}

template <typename Handler> handler_ptr_t wrap_handler(actor_base_t &actor, Handler &&handler) {
    using final_handler_t = handler_t<Handler>;
    auto handler_raw = new final_handler_t(actor, std::move(handler));
    return handler_ptr_t{handler_raw};
}

template <typename Handler> void actor_base_t::subscribe(Handler &&h) noexcept {
    supervisor.subscribe_actor(address, wrap_handler(*this, std::move(h)));
}

template <typename Handler> void actor_base_t::subscribe(Handler &&h, address_ptr_t &addr) noexcept {
    supervisor.subscribe_actor(addr, wrap_handler(*this, std::move(h)));
}

template <typename Handler> void actor_base_t::unsubscribe(Handler &&h) noexcept {
    supervisor.unsubscribe_actor(address, wrap_handler(*this, std::move(h)));
}

template <typename Handler> void actor_base_t::unsubscribe(Handler &&h, address_ptr_t &addr) noexcept {
    supervisor.unsubscribe_actor(addr, wrap_handler(*this, std::move(h)));
}

} // namespace rotor
