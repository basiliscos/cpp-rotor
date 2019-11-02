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
#include "supervisor_config.h"
#include "address_mapping.h"

#include <chrono>
#include <deque>
#include <functional>
#include <unordered_map>

namespace rotor {

namespace pt = boost::posix_time;

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
 * Supervisor is locality-aware: i.e. if two supervisors have the same
 * locality (i.e. executed in the same thread/event loop), it takes advantage
 * of this and immediately delivers message to the target supervisor
 * without involving any synchronization mechanisms. In other words,
 * a message is delivered to any actor of the locality, even if the
 * actor is not child of the current supervisor.
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

    /** \brief timer identifier type in the scope of the supervisor */
    using timer_id_t = std::uint32_t;

    /** \brief constructs new supervisor with optional parent supervisor */
    supervisor_t(supervisor_t *sup, const supervisor_config_t &config);
    supervisor_t(const supervisor_t &) = delete;
    supervisor_t(supervisor_t &&) = delete;

    virtual void do_initialize(system_context_t *ctx) noexcept override;

    /** \brief process queue of messages of locality leader
     *
     * The locality leaders queue `queue` of messages is processed.
     *
     * -# It takes message from the queue
     * -# If the message destination address belongs to the foreing the supervisor,
     * then it is forwarded to it immediately.
     * -# Otherwise, the message is local, i.e. either for the supervisor or one
     * of its non-supervisor children (internal), or to other supervisor within
     * the same locality.
     * -# in the former case the message is immediately delivered locally in
     * the context  of current supervisor; in the latter case in the context
     * of other supervsior. In the both cases `deliver_local` method is used.
     *
     * It is expected, that derived classes should invoke `do_process` message,
     * whenever it is known that there are messages for processing. The invocation
     * should be performed in safe thread/loop context.
     *
     * The method should be invoked in event-loop context only.
     *
     */
    virtual void do_process() noexcept;

    /** \brief delivers an message for self of one of child-actors  (non-supervisors)
     *
     * Supervisor iterates on subscriptions (handlers) on the message destination adddress:
     *
     * -# If the handler is local (i.e. it's actor belongs to the same supervisor),
     * -# Otherwise the message is forwarded for delivery for the foreign supervisor,
     * which owns the handler.
     *
     */
    void deliver_local(message_ptr_t &&msg) noexcept;

    /** \brief unsubcribes all actor's handlers */
    virtual void unsubscribe_actor(const actor_ptr_t &actor) noexcept;

    /** \brief creates new {@link address_t} linked with the supervisor */
    virtual address_ptr_t make_address() noexcept;

    /** \brief removes the subscription point: local address and (foreign-or-local)
     *  handler pair
     */
    virtual void commit_unsubscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept;

    /** \brief records just created actor and starts its initialization
     *
     * The initialization rquest is sent to the just created actor. If the
     * actor will not confirm initialization within timeout (specified in the message payload),
     * the actor will be asked for shut down.
     */
    virtual void on_create(message_t<payload::create_actor_t> &msg) noexcept;

    /** \brief sends {@link payload::start_actor_t} to the initialized actor  */
    virtual void on_initialize_confirm(message::init_response_t &msg) noexcept;

    virtual void on_shutdown_trigger(message::shutdown_trigger_t &) noexcept override;

    /** \brief forgets just shutted down actor
     *
     * Internal structures related to the actor are released.
     *
     */
    virtual void on_shutdown_confirm(message::shutdown_response_t &msg) noexcept;

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
    virtual void on_state_request(message::state_request_t &message) noexcept;

    /** \brief starts non-recurring timer, identified by `timer_id`
     *
     * Once timer triggers, it will invoke `on_timer_trigger(timer_id)` method;
     * othewise, if it is no longer needed, it should be cancelled via
     * `cancel_timer` method
     *
     */
    virtual void start_timer(const pt::time_duration &send, timer_id_t timer_id) noexcept = 0;

    /** \brief cancels previously started timer */
    virtual void cancel_timer(timer_id_t timer_id) noexcept = 0;

    /** \brief triggers an action associated with the timer
     *
     * Currently it just delivers response timeout, if any.
     *
     */
    virtual void on_timer_trigger(timer_id_t timer_id);

    /** \brief thread-safe version of `do_process`
     *
     * Starts supervisor to processing messages queue in safe thread/loop
     * context. Once it becomes empty, the method returns
     */
    virtual void start() noexcept = 0;

    /** \brief thread-safe version of `do_shutdown`, i.e. send shutdown request
     * let it be processed by the supervisor */
    virtual void shutdown() noexcept = 0;

    virtual void do_shutdown() noexcept override;

    virtual void shutdown_finish() noexcept override;

    /** \brief enqueues messages thread safe way and triggers processing
     *
     * This is the only method for deliver message outside of `rotor` context.
     * Basically it is `put` and `process` in the event loop context.
     *
     * The thread-safety should be guaranteed by derived class and/or used event-loop.
     *
     * This method is used for messaging between supervisors with different
     * localities, or actors which use different loops/threads.
     *
     */
    virtual void enqueue(message_ptr_t message) noexcept = 0;

    /** \brief returns pointer to parent supervisor, may be NULL */
    inline supervisor_t *get_parent_supervisor() noexcept { return parent; }

    /** \brief puts a message into internal supevisor queue for further processing
     *
     * This is thread-unsafe method. The `enqueue` method should be used to put
     * a new message from external context in thread-safe way.
     *
     */
    inline void put(message_ptr_t message) { locality_leader->queue.emplace_back(std::move(message)); }

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

    /** \brief convenient templated version of `unsubscribe_actor */
    template <typename Handler> inline void unsubscribe_actor(const address_ptr_t &addr, Handler &&handler) noexcept {
        handler_ptr_t wrapped_handler(std::forward<Handler>(handler));
        unsubscribe(wrapped_handler, addr);
    }

    /** \brief creates actor, records it in internal structures and returns
     * intrusive pointer to it
     */
    template <typename Actor, typename... Args>
    intrusive_ptr_t<Actor> create_actor(const pt::time_duration &timeout, Args... args) {
        auto &&actor = make_actor<Actor>(*this, timeout, std::forward<Args>(args)...);
        static_cast<supervisor_behavior_t *>(behavior)->on_create_child(actor->get_address());
        return actor;
    }

    /** \brief returns system context */
    inline system_context_t *get_context() noexcept { return context; }

    /** \brief convenient method for request building
     *
     * The built request isn't sent immediately, but only after invoking `send(timeout)`
     *
     */
    template <typename T, typename... Args>
    request_builder_t<T> do_request(actor_base_t &actor, const address_ptr_t &dest_addr, const address_ptr_t &reply_to,
                                    Args &&... args) noexcept {
        return request_builder_t<T>(*this, actor, dest_addr, reply_to, std::forward<Args>(args)...);
    }

    /** \brief child actror housekeeping strcuture */
    struct actor_state_t {
        /** \brief intrusive pointer to actor */
        actor_ptr_t actor;

        /** \brief whethe the shutdown request is already sent */
        bool shutdown_requesting;
    };

  protected:
    virtual actor_behavior_t *create_behavior() noexcept override;

    /** \brief creates new address with respect to supervisor locality mark */
    virtual address_ptr_t instantiate_address(const void *locality) noexcept;

    /** \brief structure to hold messages (intrusive pointers) */
    using queue_t = std::deque<message_ptr_t>;

    /** \brief address-to-subscription map type */
    using subscription_map_t = std::unordered_map<address_ptr_t, subscription_t>;

    /** \brief (local) address-to-child_actor map type */
    using actors_map_t = std::unordered_map<address_ptr_t, actor_state_t>;

    /** \brief timer to response with timeout procuder type */
    using request_map_t = std::unordered_map<timer_id_t, request_curry_t>;

    /** \brief removes actor from supervisor. It is assumed, that actor it shutted down. */
    virtual void remove_actor(actor_base_t &actor) noexcept;

    /** \brief non-owning pointer to parent supervisor, `NULL` for root supervisor */
    supervisor_t *parent;

    /** \brief non-owning pointer to system context. */
    system_context_t *context;

    /** \brief root supervisor for the locality */
    supervisor_t *locality_leader;

    /** \brief queue of unprocessed messages */
    queue_t queue;

    /** \brief local and external subscriptions for the addresses generated by the supervisor
     *
     * key: address, value: {@link subscription_t}
     *
     */
    subscription_map_t subscription_map;

    /** \brief local address to local actor (intrusive pointer) mapping */
    actors_map_t actors_map;

    /** \brief counter for request/timer ids */
    timer_id_t last_req_id;

    /** \brief timer to response with timeout procuder */
    request_map_t request_map;

    /** \brief shutdown timeout value (copied from config) */
    pt::time_duration shutdown_timeout;

    supervisor_policy_t policy;

    /** \brief per-actor and per-message request tracking support */
    address_mapping_t address_mapping;

    template <typename T> friend struct request_builder_t;
    friend struct supervisor_behavior_t;
};

using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

/* third-party classes implementations */

template <typename Supervisor, typename... Args>
auto system_context_t::create_supervisor(Args &&... args) -> intrusive_ptr_t<Supervisor> {
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

template <typename Handler> handler_ptr_t actor_base_t::subscribe(Handler &&h) noexcept {
    auto wrapped_handler = wrap_handler(*this, std::move(h));
    supervisor.subscribe_actor(address, wrapped_handler);
    return wrapped_handler;
}

template <typename Handler> handler_ptr_t actor_base_t::subscribe(Handler &&h, address_ptr_t &addr) noexcept {
    auto wrapped_handler = wrap_handler(*this, std::move(h));
    supervisor.subscribe_actor(addr, wrapped_handler);
    return wrapped_handler;
}

template <typename Handler, typename Enabled> void actor_base_t::unsubscribe(Handler &&h) noexcept {
    supervisor.unsubscribe_actor(address, wrap_handler(*this, std::move(h)));
}

template <typename Handler, typename Enabled>
void actor_base_t::unsubscribe(Handler &&h, address_ptr_t &addr) noexcept {
    supervisor.unsubscribe_actor(addr, wrap_handler(*this, std::move(h)));
}

namespace details {

template <typename Actor, typename Supervisor, typename IsSupervisor = void> struct actor_ctor_t;

/** \brief constructs new actor (derived from supervisor), SFINAE-class */
template <typename Actor, typename Supervisor>
struct actor_ctor_t<Actor, Supervisor, std::enable_if_t<std::is_base_of_v<supervisor_t, Actor>>> {

    /** \brief constructs new actor (derived from supervisor) */
    template <typename... Args>
    static auto construct(Supervisor *sup, Args... args) noexcept -> intrusive_ptr_t<Actor> {
        return new Actor{sup, std::forward<Args>(args)...};
    }
};

/** \brief constructs new actor (not derived from supervisor), SFINAE-class */
template <typename Actor, typename Supervisor>
struct actor_ctor_t<Actor, Supervisor, std::enable_if_t<!std::is_base_of_v<supervisor_t, Actor>>> {

    /** \brief constructs new actor (not derived from supervisor) */
    template <typename... Args>
    static auto construct(Supervisor *sup, Args... args) noexcept -> intrusive_ptr_t<Actor> {
        return new Actor{*sup, std::forward<Args>(args)...};
    }
};
} // namespace details

/** \brief convenience method for creating an actor in the scope of supervisor
 *
 * Actor performs early initialization, and further init will be request-based
 * and initiated  * by the supervisor.
 *
 */
template <typename Actor, typename Supervisor, typename... Args>
intrusive_ptr_t<Actor> make_actor(Supervisor &sup, const pt::time_duration &timeout, Args... args) {
    using ctor_t = details::actor_ctor_t<Actor, Supervisor>;
    auto context = sup.get_context();
    auto actor = ctor_t::construct(&sup, std::forward<Args>(args)...);
    actor->do_initialize(context);
    sup.template send<payload::create_actor_t>(sup.get_address(), actor, timeout);
    return actor;
}

template <typename T>
template <typename... Args>
request_builder_t<T>::request_builder_t(supervisor_t &sup_, actor_base_t &actor_, const address_ptr_t &destination_,
                                        const address_ptr_t &reply_to_, Args &&... args)
    : sup{sup_}, actor{actor_}, request_id{++sup.last_req_id}, destination{destination_}, reply_to{reply_to_},
      do_install_handler{false} {
    auto addr = sup.address_mapping.get_addr(actor_, response_message_t::message_type);
    if (addr) {
        imaginary_address = addr;
    } else {
        // subscribe to imaginary address instead of real one because of
        // 1. faster dispatching
        // 2. need to distinguish between "timeout guarded responses" and "responses to own requests"
        imaginary_address = sup.make_address();
        do_install_handler = true;
    }
    req.reset(new request_message_t{destination, request_id, imaginary_address, std::forward<Args>(args)...});
}

template <typename T> std::uint32_t request_builder_t<T>::send(pt::time_duration timeout) noexcept {
    if (do_install_handler) {
        install_handler();
    }
    auto fn = &request_traits_t<T>::make_error_response;
    sup.request_map.emplace(request_id, request_curry_t{fn, reply_to, req});
    sup.put(req);
    sup.start_timer(timeout, request_id);
    return request_id;
}

template <typename T> void request_builder_t<T>::install_handler() noexcept {
    auto handler = lambda<response_message_t>([supervisor = &sup](response_message_t &msg) {
        auto request_id = msg.payload.request_id();
        auto it = supervisor->request_map.find(request_id);
        if (it != supervisor->request_map.end()) {
            supervisor->cancel_timer(request_id);
            auto &orig_addr = it->second.reply_to;
            supervisor->template send<wrapped_res_t>(orig_addr, msg.payload);
            supervisor->request_map.erase(it);
        }
        // if a response to request has arrived and no timer can be found
        // that means that either timeout timer already triggered
        // and error-message already delivered or response is not expected.
        // just silently drop it anyway
    });
    auto handler_ptr = sup.subscribe(handler, imaginary_address);
    sup.address_mapping.set(actor, response_message_t::message_type, handler_ptr, imaginary_address);
}

/** \brief makes an reqest to the destination address with the message constructed from `args`
 *
 * The `reply_to` address is defaulted to actor's main address.1
 *
 */
template <typename Request, typename... Args>
request_builder_t<typename request_wrapper_t<Request>::request_t> actor_base_t::request(const address_ptr_t &dest_addr,
                                                                                        Args &&... args) {
    using request_t = typename request_wrapper_t<Request>::request_t;
    return supervisor.do_request<request_t>(*this, dest_addr, address, std::forward<Args>(args)...);
}

/** \brief makes an reqest to the destination address with the message constructed from `args`
 *
 * The `reply_addr` is used to specify the exact destinatiion address, where reply should be
 * delivered.
 *
 */
template <typename Request, typename... Args>
request_builder_t<typename request_wrapper_t<Request>::request_t>
actor_base_t::request_via(const address_ptr_t &dest_addr, const address_ptr_t &reply_addr, Args &&... args) {
    using request_t = typename request_wrapper_t<Request>::request_t;
    return supervisor.do_request<request_t>(*this, dest_addr, reply_addr, std::forward<Args>(args)...);
}

template <typename Request, typename... Args> void actor_base_t::reply_to(Request &message, Args &&... args) {
    using payload_t = typename Request::payload_t::request_t;
    using traits_t = request_traits_t<payload_t>;
    using response_t = typename traits_t::response::wrapped_t;
    using request_ptr_t = typename traits_t::request::message_ptr_t;
    send<response_t>(message.payload.reply_to, request_ptr_t{&message}, std::forward<Args>(args)...);
}

template <typename Request, typename... Args>
void actor_base_t::reply_with_error(Request &message, const std::error_code &ec) {
    using payload_t = typename Request::payload_t::request_t;
    using traits_t = request_traits_t<payload_t>;
    auto response = traits_t::make_error_response(message.payload.reply_to, message, ec);
    supervisor.put(std::move(response));
}

} // namespace rotor
