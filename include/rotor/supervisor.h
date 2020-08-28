#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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

#include <functional>
#include <unordered_map>

namespace rotor {

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

    // clang-format off
    /** \brief the default list of plugins for an supervisor
     *
     * The order of plugins is very important, as they are initialized in the direct order
     * and deinitilized in the reverse order.
     *
     */
    using plugins_list_t = std::tuple<
        plugin::address_maker_plugin_t,
        plugin::locality_plugin_t,
        plugin::delivery_plugin_t<plugin::default_local_delivery_t>,
        plugin::lifetime_plugin_t,
        plugin::init_shutdown_plugin_t,
        plugin::foreigners_support_plugin_t,
        plugin::child_manager_plugin_t,
        plugin::link_server_plugin_t,
        plugin::link_client_plugin_t,
        plugin::registry_plugin_t,
        plugin::starter_plugin_t>;
    // clang-format on

    /** \brief injects an alias for supervisor_config_t */
    using config_t = supervisor_config_t;

    /** \brief injects templated supervisor_config_builder_t */
    template <typename Supervisor> using config_builder_t = supervisor_config_builder_t<Supervisor>;

    /** \brief constructs new supervisor with optional parent supervisor */
    supervisor_t(supervisor_config_t &config);
    supervisor_t(const supervisor_t &) = delete;
    supervisor_t(supervisor_t &&) = delete;

    /** auto-start children here */
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
    inline void do_process() noexcept { delivery->process(); }

    /** \brief creates new {@link address_t} linked with the supervisor */
    virtual address_ptr_t make_address() noexcept;

    /** \brief removes the subscription point: local address and (foreign-or-local)
     *  handler pair
     */
    virtual void commit_unsubscription(const subscription_info_ptr_t &info) noexcept;

    /** \brief starts non-recurring timer, identified by `timer_id`
     *
     * Once timer triggers, it will invoke `on_timer_trigger(timer_id)` method;
     * othewise, if it is no longer needed, it should be cancelled via
     * `cancel_timer` method
     *
     */
    virtual void start_timer(const pt::time_duration &send, request_id_t timer_id) noexcept = 0;

    /** \brief cancels previously started timer */
    virtual void cancel_timer(request_id_t timer_id) noexcept = 0;

    /** \brief triggers an action associated with the timer
     *
     * Currently it just delivers response timeout, if any.
     *
     */
    virtual void on_timer_trigger(request_id_t timer_id);

    /** \brief thread-safe version of `do_process`
     *
     * Starts supervisor to processing messages queue in safe thread/loop
     * context. Once it becomes empty, the method returns
     */
    virtual void start() noexcept = 0;

    /** \brief thread-safe version of `do_shutdown`, i.e. send shutdown request
     * let it be processed by the supervisor */
    virtual void shutdown() noexcept = 0;

    void do_shutdown() noexcept override;

    /** \brief supervisor hook for reaction on child actor init */
    virtual void on_child_init(actor_base_t *actor, const std::error_code &ec) noexcept;

    /** \brief supervisor hook for reaction on child actor shutdown */
    virtual void on_child_shutdown(actor_base_t *actor, const std::error_code &ec) noexcept;

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

    /** \brief puts a message into internal supevisor queue for further processing
     *
     * This is thread-unsafe method. The `enqueue` method should be used to put
     * a new message from external context in thread-safe way.
     *
     */
    inline void put(message_ptr_t message) { locality_leader->queue.emplace_back(std::move(message)); }

    /** \brief templated version of `subscribe_actor` */
    template <typename Handler> void subscribe(actor_base_t &actor, Handler &&handler) {
        supervisor->subscribe(actor.address, wrap_handler(actor, std::move(handler)));
    }

    /** \brief convenient templated version of `unsubscribe_actor */
    template <typename Handler> inline void unsubscribe_actor(const address_ptr_t &addr, Handler &&handler) noexcept {
        handler_ptr_t wrapped_handler(std::forward<Handler>(handler));
        lifetime->unsubscribe(wrapped_handler, addr);
    }

    /** \brief creates child-actor builder */
    template <typename Actor> auto create_actor() {
        using builder_t = typename Actor::template config_builder_t<Actor>;
        assert(manager && "child_manager_plugin_t should be already initialized");
        return builder_t([this](auto &actor) { manager->create_child(actor); }, this);
    }

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
    /**
     * \brief main subscription implementation
     *
     * The subscription point is materialized inot subscription info. If address is
     * internal/local, then it is immediately confirmed to the source actor as
     * {@link payload::subscription_confirmation_t}.
     *
     * Otherwise, if the address is external (foreign), then subscription request
     * is forwarded to approriate supervisor as {@link payload::external_subscription_t}
     * request.
     *
     * The materialized subscription info is returned.
     *
     */
    subscription_info_ptr_t subscribe(const handler_ptr_t &handler, const address_ptr_t &addr,
                                      const actor_base_t *owner_ptr, owner_tag_t owner_tag) noexcept;

    using actor_base_t::subscribe;

    /** \brief returns registry actor address (if it was defined or registry actor was created) */
    inline const address_ptr_t &get_registry_address() const noexcept { return registry_address; }

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

    /** \brief generic non-public methods accessor */
    template <typename T, typename... Args> auto access(Args... args) noexcept;

  protected:
    /** \brief creates new address with respect to supervisor locality mark */
    virtual address_ptr_t instantiate_address(const void *locality) noexcept;

    /* \brief address-to-subscription map type */

    /** \brief timer to response with timeout procuder type */
    using request_map_t = std::unordered_map<request_id_t, request_curry_t>;

    /** \brief non-owning pointer to system context. */
    system_context_t *context;

    /** \brief queue of unprocessed messages */
    messages_queue_t queue;

    /** \brief counter for request/timer ids */
    request_id_t last_req_id;

    /** \brief timer to response with timeout procuder */
    request_map_t request_map;

    /** \brief main subscription support class  */
    subscription_t subscription_map;

    /** \brief non-owning pointer to parent supervisor, `NULL` for root supervisor */
    supervisor_t *parent;

    /** \brief delivery plugin pointer */
    plugin::delivery_plugin_base_t *delivery = nullptr;

    /** \brief child manager plugin pointer */
    plugin::child_manager_plugin_t *manager = nullptr;

    /** \brief root supervisor for the locality */
    supervisor_t *locality_leader;

  private:
    bool create_registry;
    bool synchronize_start;
    address_ptr_t registry_address;

    supervisor_policy_t policy;

    /** \brief per-actor and per-message request tracking support */
    address_mapping_t address_mapping;

    template <typename T> friend struct request_builder_t;
    template <typename Supervisor> friend struct actor_config_builder_t;
    friend struct plugin::delivery_plugin_base_t;
    template <typename T> friend struct plugin::delivery_plugin_t;

    inline request_id_t next_request_id() noexcept {
        request_map_t::iterator it;
        do {
            it = request_map.find(++last_req_id);
        } while (it != request_map.end());
        return last_req_id;
    }
};

using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

/* third-party classes implementations */

template <typename Supervisor> auto system_context_t::create_supervisor() {
    using builder_t = typename Supervisor::template config_builder_t<Supervisor>;
    return builder_t(
        [this](auto &actor) {
            if (supervisor) {
                on_error(make_error_code(error_code_t::supervisor_defined));
                actor.reset();
            } else {
                this->supervisor = actor;
                actor->do_initialize(this);
            }
        },
        *this);
}

template <typename M, typename... Args> void actor_base_t::send(const address_ptr_t &addr, Args &&... args) {
    supervisor->put(make_message<M>(addr, std::forward<Args>(args)...));
}

/** \brief wraps handler (pointer to member function) and actor address into intrusive pointer */
template <typename Handler> handler_ptr_t wrap_handler(actor_base_t &actor, Handler &&handler) {
    using final_handler_t = handler_t<Handler>;
    auto handler_raw = new final_handler_t(actor, std::move(handler));
    return handler_ptr_t{handler_raw};
}

template <typename Handler> subscription_info_ptr_t actor_base_t::subscribe(Handler &&h) noexcept {
    auto wrapped_handler = wrap_handler(*this, std::move(h));
    return supervisor->subscribe(wrapped_handler, address, this, owner_tag_t::ANONYMOUS);
}

template <typename Handler>
subscription_info_ptr_t actor_base_t::subscribe(Handler &&h, const address_ptr_t &addr) noexcept {
    auto wrapped_handler = wrap_handler(*this, std::move(h));
    return supervisor->subscribe(wrapped_handler, addr, this, owner_tag_t::ANONYMOUS);
}

namespace plugin {

template <typename Handler>
subscription_info_ptr_t plugin_base_t::subscribe(Handler &&h, const address_ptr_t &addr) noexcept {
    using final_handler_t = handler_t<Handler>;
    handler_ptr_t wrapped_handler(new final_handler_t(*this, std::move(h)));
    auto info = actor->supervisor->subscribe(wrapped_handler, addr, actor, owner_tag_t::PLUGIN);
    own_subscriptions.emplace_back(info);
    return info;
}

template <typename Handler> subscription_info_ptr_t plugin_base_t::subscribe(Handler &&h) noexcept {
    return subscribe(std::forward<Handler>(h), actor->address);
}

template <> inline auto &plugin_base_t::access<plugin::starter_plugin_t>() noexcept { return own_subscriptions; }

template <typename Handler> handler_ptr_t starter_plugin_t::subscribe_actor(Handler &&handler) noexcept {
    auto &address = actor->get_address();
    return subscribe_actor(std::forward<Handler>(handler), address);
}

template <typename Handler>
handler_ptr_t starter_plugin_t::subscribe_actor(Handler &&handler, const address_ptr_t &addr) noexcept {
    auto wrapped_handler = wrap_handler(*actor, std::move(handler));
    auto info = actor->get_supervisor().subscribe(wrapped_handler, addr, actor, owner_tag_t::PLUGIN);
    assert(std::count_if(tracked.begin(), tracked.end(), [&](auto &it) { return *it == *info; }) == 0 &&
           "already subscribed");
    tracked.emplace_back(info);
    access<starter_plugin_t>().emplace_back(std::move(info));
    return wrapped_handler;
}

template <typename LocalDelivery> void delivery_plugin_t<LocalDelivery>::process() noexcept {
    while (queue->size()) {
        auto message = queue->front();
        auto &dest = message->address;
        queue->pop_front();
        auto &dest_sup = dest->supervisor;
        auto internal = &dest_sup == actor;
        if (internal) { /* subscriptions are handled by me */
            auto *local_recipients = subscription_map->get_recipients(*message);
            if (local_recipients) {
                LocalDelivery::delivery(message, *local_recipients);
            }
        } else if (dest_sup.address->same_locality(*address)) {
            auto *local_recipients = dest_sup.subscription_map.get_recipients(*message);
            if (local_recipients) {
                LocalDelivery::delivery(message, *local_recipients);
            }
        } else {
            dest_sup.enqueue(std::move(message));
        }
    }
}

} // namespace plugin

template <typename Handler, typename Enabled> void actor_base_t::unsubscribe(Handler &&h) noexcept {
    supervisor->unsubscribe_actor(address, wrap_handler(*this, std::move(h)));
}

template <typename Handler, typename Enabled>
void actor_base_t::unsubscribe(Handler &&h, address_ptr_t &addr) noexcept {
    supervisor->unsubscribe_actor(addr, wrap_handler(*this, std::move(h)));
}

template <typename T>
template <typename... Args>
request_builder_t<T>::request_builder_t(supervisor_t &sup_, actor_base_t &actor_, const address_ptr_t &destination_,
                                        const address_ptr_t &reply_to_, Args &&... args)
    : sup{sup_}, actor{actor_}, request_id{sup.next_request_id()}, destination{destination_}, reply_to{reply_to_},
      do_install_handler{false} {
    auto addr = sup.address_mapping.get_mapped_address(actor_, response_message_t::message_type);
    if (addr) {
        imaginary_address = addr;
    } else {
        // subscribe to imaginary address instead of real one because of
        // 1. faster dispatching
        // 2. need to distinguish between "timeout guarded responses" and "responses to own requests"
        imaginary_address = sup.make_address();
        do_install_handler = true;
    }
    req.reset(
        new request_message_t{destination, request_id, imaginary_address, reply_to_, std::forward<Args>(args)...});
}

template <typename T> request_id_t request_builder_t<T>::send(pt::time_duration timeout) noexcept {
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
            auto &orig_addr = it->second.origin;
            supervisor->template send<wrapped_res_t>(orig_addr, msg.payload);
            supervisor->request_map.erase(it);
        }
        // if a response to request has arrived and no timer can be found
        // that means that either timeout timer already triggered
        // and error-message already delivered or response is not expected.
        // just silently drop it anyway
    });
    auto wrapped_handler = wrap_handler(sup, std::move(handler));
    auto info = sup.subscribe(wrapped_handler, imaginary_address, &actor, owner_tag_t::SUPERVISOR);
    sup.address_mapping.set(actor, info);
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
    return supervisor->do_request<request_t>(*this, dest_addr, address, std::forward<Args>(args)...);
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
    return supervisor->do_request<request_t>(*this, dest_addr, reply_addr, std::forward<Args>(args)...);
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
    supervisor->put(std::move(response));
}

template <typename Actor>
actor_config_builder_t<Actor>::actor_config_builder_t(install_action_t &&action_, supervisor_t *supervisor_)
    : install_action{std::move(action_)}, supervisor{supervisor_},
      system_context{*supervisor_->context}, config{supervisor_} {
    init_ctor();
}

template <typename Actor> intrusive_ptr_t<Actor> actor_config_builder_t<Actor>::finish() && {
    intrusive_ptr_t<Actor> actor_ptr;
    if (!validate()) {
        auto ec = make_error_code(error_code_t::actor_misconfigured);
        system_context.on_error(ec);
    } else {
        auto &cfg = static_cast<typename builder_t::config_t &>(config);
        auto actor = new Actor(cfg);
        actor_ptr.reset(actor);
        install_action(actor_ptr);
    }
    return actor_ptr;
}

} // namespace rotor
