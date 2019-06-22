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

template <typename Actor, typename Supervisor, typename IsSupervisor = void> struct actor_ctor_t;

template <typename Actor, typename Supervisor>
struct actor_ctor_t<Actor, Supervisor, std::enable_if_t<std::is_base_of_v<supervisor_t, Actor>>> {
    template <typename... Args>
    static auto construct(Supervisor *sup, Args... args) noexcept -> intrusive_ptr_t<Actor> {
        return new Actor{sup, std::forward<Args>(args)...};
    }
};

template <typename Actor, typename Supervisor>
struct actor_ctor_t<Actor, Supervisor, std::enable_if_t<!std::is_base_of_v<supervisor_t, Actor>>> {
    template <typename... Args>
    static auto construct(Supervisor *sup, Args... args) noexcept -> intrusive_ptr_t<Actor> {
        return new Actor{*sup, std::forward<Args>(args)...};
    }
};

template <typename Actor, typename Supervisor, typename... Args>
intrusive_ptr_t<Actor> make_actor(Supervisor &sup, Args... args);

struct supervisor_t : public actor_base_t {

    supervisor_t(supervisor_t *sup = nullptr);
    supervisor_t(const supervisor_t &) = delete;
    supervisor_t(supervisor_t &&) = delete;

    virtual void do_initialize(system_context_t *ctx) noexcept override;
    virtual void do_start() noexcept;
    virtual void do_process() noexcept;

    // virtual void proccess_subscriptions() noexcept;

    virtual size_t unsubscribe_actor(const actor_ptr_t &actor) noexcept;
    virtual address_ptr_t make_address() noexcept;
    virtual void commit_unsubscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept;
    virtual void remove_actor(actor_base_t &actor) noexcept;
    virtual void confirm_shutdown() noexcept override;

    virtual void on_create(message_t<payload::create_actor_t> &msg) noexcept;
    virtual void on_initialize(message_t<payload::initialize_actor_t> &msg) noexcept override;
    virtual void on_initialize_confirm(message_t<payload::initialize_confirmation_t> &msg) noexcept;

    virtual void on_shutdown(message_t<payload::shutdown_request_t> &) noexcept override;
    virtual void on_shutdown_confirm(message_t<payload::shutdown_confirmation_t> &message) noexcept;

    virtual void on_external_subs(message_t<payload::external_subscription_t> &message) noexcept;
    virtual void on_commit_unsubscription(message_t<payload::commit_unsubscription_t> &message) noexcept;
    virtual void on_call(message_t<payload::handler_call_t> &message) noexcept;
    virtual void on_state_request(message_t<payload::state_request_t> &message) noexcept;

    virtual void on_shutdown_timer_trigger() noexcept;
    virtual void start_shutdown_timer() noexcept = 0;
    virtual void cancel_shutdown_timer() noexcept = 0;
    virtual void start() noexcept = 0;
    virtual void shutdown() noexcept = 0;
    virtual void enqueue(message_ptr_t message) noexcept = 0;

    inline supervisor_t *get_parent_supevisor() noexcept { return parent; }

    using queue_t = std::deque<message_ptr_t>;
    using subscription_map_t = std::unordered_map<address_ptr_t, subscription_t>;
    using actors_map_t = std::unordered_map<address_ptr_t, actor_ptr_t>;

    supervisor_t *parent;
    system_context_t *context;
    queue_t outbound;
    subscription_map_t subscription_map;
    actors_map_t actors_map;

    inline void put(message_ptr_t message) { outbound.emplace_back(std::move(message)); }

    template <typename Handler> void subscribe_actor(actor_base_t &actor, Handler &&handler) {
        supervisor.subscribe_actor(actor.get_address(), wrap_handler(actor, std::move(handler)));
    }

    inline void subscribe_actor(const address_ptr_t &addr, const handler_ptr_t &handler) {
        if (&addr->supervisor == &supervisor) {
            auto subs_info = subscription_map.try_emplace(addr, *this);
            subs_info.first->second.subscribe(handler);
            send<payload::subscription_confirmation_t>(handler->actor_addr, addr, handler);
        } else {
            send<payload::external_subscription_t>(addr->supervisor.address, addr, handler);
        }
    }

    template <typename Handler> inline void unsubscribe_actor(const address_ptr_t &addr, Handler &&handler) noexcept {
        auto &dest = handler->actor_addr;
        if (&addr->supervisor == &supervisor) {
            send<payload::unsubscription_confirmation_t>(dest, addr, std::forward<Handler>(handler));
        } else {
            send<payload::external_unsubscription_t>(dest, addr, std::forward<Handler>(handler));
        }
    }

    template <typename Actor, typename... Args> intrusive_ptr_t<Actor> create_actor(Args... args) {
        return make_actor<Actor>(*this, std::forward<Args>(args)...);
    }
};

using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

/* third-party classes implementations */

template <typename M, typename... Args> auto make_message(const address_ptr_t &addr, Args &&... args) -> message_ptr_t {
    return message_ptr_t{new message_t<M>(addr, std::forward<Args>(args)...)};
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

template <typename Actor, typename Supervisor, typename... Args>
intrusive_ptr_t<Actor> make_actor(Supervisor &sup, Args... args) {
    using ctor_t = actor_ctor_t<Actor, Supervisor>;
    if (sup.get_state() != state_t::INITIALIZED)
        sup.context->on_error(make_error_code(error_code_t::supervisor_wrong_state));
    auto actor = ctor_t::construct(&sup, std::forward<Args>(args)...);
    actor->do_initialize(sup.context);
    sup.template send<payload::create_actor_t>(sup.get_address(), actor);
    return actor;
}

} // namespace rotor
