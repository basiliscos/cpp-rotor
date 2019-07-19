#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

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

/** \brief constucts actor on the supervisor */
template <typename Actor, typename Supervisor, typename... Args>
intrusive_ptr_t<Actor> make_actor(Supervisor &sup, Args... args);

/** \struct supervisor_t
 *  \brief supervisor is responsible for managing actors (workers) lifetime
 *
 * Supervisor starts, stops actors (children/workers) and process messages.
 * The message processing is basically sorting messages by their destination
 * {@link address_t}: if an address belongs to the supervisor, then message
 * is dispatched locally, otherwise the message is forwarded to supervisor,
 * which owns address.
 *
 * During message dispatching phase, supervisor examines handlers
 * ({@link handler_base_t}), if they are local, then a message in immediately
 * delivered to it (i.e. a local actor is invoked immediately), otherwise
 * is is forwarded for delivery to the supervisor, which owns the handler.
 *
 * Supervisor is responsible for managing it's local actors lifetime, i.e.
 * sending initialization, start, shutdown requests etc.
 *
 * As supervisor is special kind of actor, it should be possible to spawn
 * other supervisors constructing tree-like organization of responsibilities.
 *
 * Unlike Erlang's supervisor, rotor's supervisor does not spawn actors
 * if they terminated. It should be, hovewer, to implement it in derived
 * classes with application-specific logic.
 *
 * This supervisor class is abstract, and the concrete implementation is
 * is event-loop specific, i.e. it should know how to start/stop shutdown
 * timers, how to trigger messages processing in thread-safe way, how
 * to deliver message to a supervisor in a thread-safe way etc.
 *
 */
struct supervisor_t : public actor_base_t {
    /** \brief constructs new supervisor with optional parent supervisor */
    supervisor_t(supervisor_t *sup = nullptr);
    supervisor_t(const supervisor_t &) = delete;
    supervisor_t(supervisor_t &&) = delete;

    virtual void do_initialize(system_context_t *ctx) noexcept override;

    /** \brief convenient method to send {@link payload::start_actor_t}
     * message to supervisor itself.
     */
    virtual void do_start() noexcept;

    /** \brief process internal messages queue and exists when it's empty
     *
     * -# It takes message from the queue
     * -# If the message destination address belongs to the foreing the supervisor,
     * then it is forwarded to it immediately
     * -# Otherwise it iterates on subscriptions (handlers) on the adddress
     * -# If the handler is local (i.e. it's actor belongs to the same supervisor),
     * the message is delivered immediately to it
     * -# Otherwise it is forwareded for delivery for the foreign supervisor,
     * which owns the handler.
     * -# When queue is empty, it just exits
     *
     * It is expected, that derived classes should invoke `do_process` message,
     * whenever it is known that there are messages for processing.
     *
     * The method should be invoked in event-loop context only.
     *
     */
    virtual void do_process() noexcept;

    /** \brief unsubcribes all actor's handlers */
    virtual void unsubscribe_actor(const actor_ptr_t &actor) noexcept;

    /** \brief creates new {@link address_t} linked with the supervisor */
    virtual address_ptr_t make_address() noexcept;

    /** \brief removes the subscription point: local address and (foreign-or-local)
     *  handler pair
     */
    virtual void commit_unsubscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept;

    /** \brief sends {@link payload::shutdown_confirmation_t} to parent
     * supervisor if it does present
     */
    virtual void confirm_shutdown() noexcept override;

    /** \brief records just created actor and starts its initialization  */
    virtual void on_create(message_t<payload::create_actor_t> &msg) noexcept;

    virtual void on_initialize(message_t<payload::initialize_actor_t> &msg) noexcept override;

    /** \brief sends {@link payload::start_actor_t} to the initialized actor  */
    virtual void on_initialize_confirm(message_t<payload::initialize_confirmation_t> &msg) noexcept;

    virtual void on_shutdown(message_t<payload::shutdown_request_t> &) noexcept override;

    /** \brief forgets just shutted down actor */
    virtual void on_shutdown_confirm(message_t<payload::shutdown_confirmation_t> &message) noexcept;

    /** \brief subscribes external handler to local address */
    virtual void on_external_subs(message_t<payload::external_subscription_t> &message) noexcept;

    /** \brief message interface for `commit_unsubscription` */
    virtual void on_commit_unsubscription(message_t<payload::commit_unsubscription_t> &message) noexcept;

    /** \brief delivers a message to local handler, which was originally send to external address
     *
     * The handler is subscribed to the external address, that's why the message was forwarded
     * from external supervisor to the local supervisor to process the call (invoke the local handler).
     *
     */
    virtual void on_call(message_t<payload::handler_call_t> &message) noexcept;

    /** \brief answers about actor's state, identified by it's address
     *
     * If there is no information about the address (including the case when an actor
     * is not yet created or already destroyed), then it replies with `UNKNOWN` status.
     *
     * It replies to the address specified to the `reply_addr` specified in
     * the message {@link payload::state_request_t}.
     *
     */
    virtual void on_state_request(message_t<payload::state_request_t> &message) noexcept;

    /** \brief default reaction on shutdown timer trigger
     *
     * If shutdown timer triggers it is treated as fatal error, which is printed
     * to `stdout` followed by `std::abort`
     *
     */
    virtual void on_shutdown_timer_trigger() noexcept;

    /** \brief starts event-loop specific shutdown timer */
    virtual void start_shutdown_timer() noexcept = 0;

    /** \brief cancels event-loop specific shutdown timer */
    virtual void cancel_shutdown_timer() noexcept = 0;

    /** \brief thread-safe version of `do_start`, i.e. send start actor request
     * let it be processed by the supervisor */
    virtual void start() noexcept = 0;

    /** \brief thread-safe version of `do_shutdown`, i.e. send shutdown request
     * let it be processed by the supervisor */
    virtual void shutdown() noexcept = 0;

    /** \brief enqueues messages thread safe way and triggers processing
     *
     * This is the only method for deliver message outside of `rotor` context.
     * Basically it is `put` and `process` in the event loop context.
     *
     * The thread-safety should be guaranteed by derived class and/or used event-loop.
     *
     */
    virtual void enqueue(message_ptr_t message) noexcept = 0;

    /** \brief returns pointer to parent supervisor */
    inline supervisor_t *get_parent_supervisor() noexcept { return parent; }

    /** \brief puts a message into internal supevisor queue for further processing
     *
     * This is thread-unsafe method. The `enqueue` method should be used to put
     * a new message from external context in thread-safe way.
     *
     */
    inline void put(message_ptr_t message) { outbound.emplace_back(std::move(message)); }

    /**
     * \brief subscribes an handler to an address.
     *
     * If the address is local, then subscription point is recorded and
     * {@link payload::subscription_confirmation_t} is send to the handler's actor.
     *
     * Otherwise, if the address is external (foreign), then subscription request
     * is forwarded to approriate supervisor as {@link payload::external_subscription_t}
     * request.
     *
     */
    inline void subscribe_actor(const address_ptr_t &addr, const handler_ptr_t &handler) {
        if (&addr->supervisor == &supervisor) {
            auto subs_info = subscription_map.try_emplace(addr, *this);
            subs_info.first->second.subscribe(handler);
            send<payload::subscription_confirmation_t>(handler->actor_ptr->get_address(), addr, handler);
        } else {
            send<payload::external_subscription_t>(addr->supervisor.address, addr, handler);
        }
    }

    /** \brief templated version of `subscribe_actor` */
    template <typename Handler> void subscribe_actor(actor_base_t &actor, Handler &&handler) {
        supervisor.subscribe_actor(actor.get_address(), wrap_handler(actor, std::move(handler)));
    }

    /** \brief unsubscribes local handler from the address
     *
     * If the address is local, then unsubscription confirmation is sent immediately,
     * otherwise {@link payload::external_subscription_t} request is sent to the external
     * supervisor, which owns the address.
     *
     */
    template <typename Handler> inline void unsubscribe_actor(const address_ptr_t &addr, Handler &&handler) noexcept {
        auto &dest = handler->actor_ptr->address;
        if (&addr->supervisor == &supervisor) {
            send<payload::unsubscription_confirmation_t>(dest, addr, std::forward<Handler>(handler));
        } else {
            send<payload::external_unsubscription_t>(dest, addr, std::forward<Handler>(handler));
        }
    }

    /** \brief creates actor, records it in internal structures and returns
     * intrusive pointer ot the actors
     */
    template <typename Actor, typename... Args> intrusive_ptr_t<Actor> create_actor(Args... args) {
        return make_actor<Actor>(*this, std::forward<Args>(args)...);
    }

    /** \brief returns system context */
    inline system_context_t *get_context() noexcept { return context; }

  protected:
    /** \brief structure to hold messages (intrusive pointers) */
    using queue_t = std::deque<message_ptr_t>;

    /** \brief address-to-subscription map type */
    using subscription_map_t = std::unordered_map<address_ptr_t, subscription_t>;

    /** \brief (local) address-to-actor map type */
    using actors_map_t = std::unordered_map<address_ptr_t, actor_ptr_t>;

    /** \brief removes actor from supervisor. It is assumed, that actor it shutted down. */
    virtual void remove_actor(actor_base_t &actor) noexcept;

    /** \brief non-owning pointer to parent supervisor, `NULL` for root supervisor */
    supervisor_t *parent;

    /** \brief non-owning pointer to system context. */
    system_context_t *context;

    /** \brief queue of unprocessed messages */
    queue_t outbound;

    /** \brief local and external subscriptions for the addresses generated by the supervisor
     *
     * key: address, value: {@link subscription_t}
     *
     */
    subscription_map_t subscription_map;

    /** \brief local address to local actor (intrusive pointer) mapping */
    actors_map_t actors_map;
};

using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

/* third-party classes implementations */

/** \brief constucts message by constructing it's payload; intrusive pointer for the message is returned */
template <typename M, typename... Args> auto make_message(const address_ptr_t &addr, Args &&... args) -> message_ptr_t {
    return message_ptr_t{new message_t<M>(addr, std::forward<Args>(args)...)};
}

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

/** \brief wraps handler (pointer to member function) and actor address into intrusive pointer */
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

namespace details {

template <typename Actor, typename Supervisor, typename IsSupervisor = void> struct actor_ctor_t;

/** \brief constructs new actor (derived from supervisor), SFINAE-class */
template <typename Actor, typename Supervisor>
struct actor_ctor_t<Actor, Supervisor, std::enable_if_t<std::is_base_of_v<supervisor_t, Actor>>> {
    template <typename... Args>
    static auto construct(Supervisor *sup, Args... args) noexcept -> intrusive_ptr_t<Actor> {
        /** \brief constructs new actor (derived from supervisor) */
        return new Actor{sup, std::forward<Args>(args)...};
    }
};

/** \brief constructs new actor (not derived from supervisor), SFINAE-class */
template <typename Actor, typename Supervisor>
struct actor_ctor_t<Actor, Supervisor, std::enable_if_t<!std::is_base_of_v<supervisor_t, Actor>>> {
    template <typename... Args>
    /** \brief constructs new actor (not derived from supervisor) */
    static auto construct(Supervisor *sup, Args... args) noexcept -> intrusive_ptr_t<Actor> {
        return new Actor{*sup, std::forward<Args>(args)...};
    }
};
} // namespace details

template <typename Actor, typename Supervisor, typename... Args>
intrusive_ptr_t<Actor> make_actor(Supervisor &sup, Args... args) {
    using ctor_t = details::actor_ctor_t<Actor, Supervisor>;
    auto state = sup.get_state();
    auto context = sup.get_context();
    if ((state != state_t::INITIALIZED) && (state != state_t::OPERATIONAL)) {
        context->on_error(make_error_code(error_code_t::supervisor_wrong_state));
    }
    auto actor = ctor_t::construct(&sup, std::forward<Args>(args)...);
    actor->do_initialize(context);
    sup.template send<payload::create_actor_t>(sup.get_address(), actor);
    return actor;
}

} // namespace rotor
