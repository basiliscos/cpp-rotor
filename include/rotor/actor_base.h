#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "forward.hpp"
#include "address.hpp"
#include "actor_config.h"
#include "messages.hpp"
#include "state.h"
#include "handler.h"
#include "extended_error.h"
#include "timer_handler.hpp"
#include <set>

namespace rotor {

/** \struct actor_base_t
 *  \brief universal primitive of concurrent computation
 *
 *  The class is base class for user-defined actors. It is expected that
 * actors will react on incoming messages (e.g. by changing internal
 * /private state) or send (other) messages to other actors, or do
 * some side-effects (I/O, etc.).
 *
 * Message passing interface is asynchronous, they are send to {@link supervisor_t}.
 *
 * Every actor belong to some {@link supervisor_t}, which "injects" the thread-safe
 * execution context, in a sense, that the actor can call it's own methods as well
 * as supervisors without any need of synchonization.
 *
 * All actor methods are thread-unsafe, i.e. should not be called with except of
 * it's own supervisor. Communication with actor should be performed via messages.
 *
 * Actor is addressed by it's "main" address; however it is possible for an actor
 * to have multiple identities aka "virtual" addresses.
 *
 */
struct actor_base_t : public arc_base_t<actor_base_t> {
    /** \brief injects an alias for actor_config_t */
    using config_t = actor_config_t;

    /** \brief injects templated actor_config_builder_t */
    template <typename Actor> using config_builder_t = actor_config_builder_t<Actor>;

    /** \brief SFINAE handler detector
     *
     * Either handler can be constructed from  memeber-to-function-pointer or
     * it is already constructed and have a base `handler_base_t`
     */
    template <typename Handler>
    using is_handler =
        std::enable_if_t<std::is_member_function_pointer_v<Handler> || std::is_base_of_v<handler_base_t, Handler>>;

    // clang-format off
    /** \brief the default list of plugins for an actor
     *
     * The order of plugins is very important, as they are initialized in the direct order
     * and deinitilized in the reverse order.
     *
     */
    using plugins_list_t = std::tuple<
        plugin::address_maker_plugin_t,
        plugin::lifetime_plugin_t,
        plugin::init_shutdown_plugin_t,
        plugin::link_server_plugin_t,
        plugin::link_client_plugin_t,
        plugin::registry_plugin_t,
        plugin::resources_plugin_t,
        plugin::starter_plugin_t>;
    // clang-format on

    /** \brief constructs actor and links it's supervisor
     *
     * An actor cannot outlive it's supervisor.
     *
     * Sets internal actor state to `NEW`
     *
     */
    actor_base_t(config_t &cfg);
    virtual ~actor_base_t();

    /** \brief early actor initialization (pre-initialization)
     *
     * Actor's plugins are activated, "main" address is created
     * (via {@link plugin::address_maker_plugin_t}), state is set
     * to `INITIALIZING` (via {@link plugin::init_shutdown_plugin_t}).
     *
     */
    virtual void do_initialize(system_context_t *ctx) noexcept;

    /** \brief convenient method to send actor's supervisor shutdown trigger message
     *
     * The reason will be send "as is"
     */
    virtual void do_shutdown(const extended_error_ptr_t &reason) noexcept;

    /** \brief convenient method to send actor's supervisor shutdown trigger message
     *
     *  The context will be set to the current actor identity
     */
    void do_shutdown(std::error_code &ec, const extended_error_ptr_t &cause = {}) noexcept;

    /** \brief actor is fully initialized and it's supervisor has sent signal to start
     *
     * The actor state is set to `OPERATIONAL`.
     *
     */
    virtual void on_start() noexcept;

    /** \brief sends message to the destination address
     *
     * Internally it just constructs new message in supervisor's outbound queue.
     *
     */
    template <typename M, typename... Args> void send(const address_ptr_t &addr, Args &&...args);

    /** \brief returns request builder for destination address using the "main" actor address
     *
     * The `args` are forwarded for construction of the request. The request is not actually sent,
     * until `send` method of {@link request_builder_t} will be invoked.
     *
     * Supervisor will spawn timeout timer upon `timeout` method.
     */
    template <typename R, typename... Args>
    request_builder_t<typename request_wrapper_t<R>::request_t> request(const address_ptr_t &dest_addr, Args &&...args);

    /** \brief returns request builder for destination address using the specified address for reply
     *
     * It is assumed, that the specified address belongs to the actor.
     *
     * The method is useful, when a different behavior is needed for the same
     * message response types. It serves at some extend as virtual dispatching within
     * the actor.
     *
     * See the description of `request` method.
     *
     */
    template <typename R, typename... Args>
    request_builder_t<typename request_wrapper_t<R>::request_t>
    request_via(const address_ptr_t &dest_addr, const address_ptr_t &reply_addr, Args &&...args);

    /** \brief convenient method for constructing and sending response to a request
     *
     * `args` are forwarded to response payload constuction
     */
    template <typename Request, typename... Args> void reply_to(Request &message, Args &&...args);

    /** \brief convenient method for constructing and sending error response to a request */
    template <typename Request> void reply_with_error(Request &message, const extended_error_ptr_t &ec);

    /** \brief makes response to the request, but does not send it.
     *
     * The return type is intrusive pointer to the message, not the message itself.
     *
     * It can be useful for delayed responses. The response can be dispatched later via
     * supervisor->put(std::move(response_ptr));
     *
     */
    template <typename Request, typename... Args> auto make_response(Request &message, Args &&...args);

    /** \brief makes error response to the request, but does not send it.
     *
     * The return type is intrusive pointer to the message, not the message itself.
     *
     * It can be useful for delayed responses. The response can be dispatched later via
     * supervisor->put(std::move(response_ptr));
     *
     */
    template <typename Request> auto make_response(Request &message, const extended_error_ptr_t &ec);

    /** \brief subscribes actor's handler to process messages on the specified address */
    template <typename Handler> subscription_info_ptr_t subscribe(Handler &&h, const address_ptr_t &addr) noexcept;

    /** \brief subscribes actor's handler to process messages on the actor's "main" address */
    template <typename Handler> subscription_info_ptr_t subscribe(Handler &&h) noexcept;

    /** \brief unsubscribes actor's handler from process messages on the specified address */
    template <typename Handler, typename = is_handler<Handler>>
    void unsubscribe(Handler &&h, address_ptr_t &addr) noexcept;

    /** \brief unsubscribes actor's handler from processing messages on the actor's "main" address */
    template <typename Handler, typename = is_handler<Handler>> void unsubscribe(Handler &&h) noexcept;

    /* \brief initiates handler unsubscription from the address
     *
     * If the address is local, then unsubscription confirmation is sent immediately,
     * otherwise {@link payload::external_subscription_t} request is sent to the external
     * supervisor, which owns the address.
     *
     * The optional call can be providded to be called upon message destruction.
     *
     */

    /** \brief initiates handler unsubscription from the default actor address */
    inline void unsubscribe(const handler_ptr_t &h) noexcept { lifetime->unsubscribe(h, address); }

    /** \brief starts plugins activation */
    void activate_plugins() noexcept;

    /** \brief finishes plugin activation, successful or not */
    void commit_plugin_activation(plugin::plugin_base_t &plugin, bool success) noexcept;

    /** \brief starts plugins deactivation */
    void deactivate_plugins() noexcept;

    /** \brief finishes plugin deactivation */
    void commit_plugin_deactivation(plugin::plugin_base_t &plugin) noexcept;

    /** \brief propaagtes subscription message to corresponding actors */
    void on_subscription(message::subscription_t &message) noexcept;

    /** \brief propaagtes unsubscription message to corresponding actors */
    void on_unsubscription(message::unsubscription_t &message) noexcept;

    /** \brief propaagtes external unsubscription message to corresponding actors */
    void on_unsubscription_external(message::unsubscription_external_t &message) noexcept;

    /** \brief creates new unique address for an actor (via address_maker plugin) */
    address_ptr_t create_address() noexcept;

    /** \brief starts shutdown procedure, e.g. upon receiving shutdown request
     *
     * The actor state is set to SHUTTING_DOWN.
     *
     */
    virtual void shutdown_start() noexcept;

    /** \brief polls plugins for shutdown
     *
     * The poll is performed in the reverse order. If all plugins, with active
     * shutdown reaction confirm they are ready to shutdown, then the
     * `shutdown_finish` method is invoked.
     *
     */
    void shutdown_continue() noexcept;

    /** \brief finalizes shutdown
     *
     * The shutdown response is sent and actor state is set to SHUT_DOWN.
     *
     * This is the last action in the shutdown sequence.
     * No further methods will be invoked on the actor
     *
     */
    virtual void shutdown_finish() noexcept;

    /** \brief starts initialization procedure
     *
     * The actor state is set to INITIALIZING.
     *
     */
    virtual void init_start() noexcept;

    /** \brief polls plugins whether they completed initialization.
     *
     * The poll is performed in the direct order. If all plugins, with active
     * init reaction confirm they are ready, then the `init_finish` method
     * is invoked.
     *
     */
    void init_continue() noexcept;

    /** \brief finalizes initialization
     *
     * The init response is sent and actor state is set to INITIALIZED.
     *
     */
    virtual void init_finish() noexcept;

    /** \brief main callback for plugin configuration when it's ready */
    virtual void configure(plugin::plugin_base_t &plugin) noexcept;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

    /** \brief generic non-public methods accessor */
    template <typename T, typename... Args> auto access(Args... args) noexcept;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() const noexcept;

    /** \brief generic non-public methods accessor */
    template <typename T, typename... Args> auto access(Args... args) const noexcept;

    /** \brief returns actor's main address */
    inline const address_ptr_t &get_address() const noexcept { return address; }

    /** \brief returns actor's supervisor */
    inline supervisor_t &get_supervisor() const noexcept { return *supervisor; }

    /** \brief spawns a new one-shot timer
     *
     * \param interval specifies amount of time, after which the timer will trigger.
     * \param delegate is an object of arbitrary class.
     * \param method is the pointer-to-member-function of the object, which will be
     * invoked upon timer triggering or cancellation.
     *
     * The `method` parameter should have the following signature:
     *
     * void Delegate::on_timer(request_id_t, bool cancelled) noexcept;
     *
     * `start_timer` returns timer identity. It will be supplied to the specified callback,
     * or the timer can be cancelled via it.
     */
    template <typename Delegate, typename Method>
    request_id_t start_timer(const pt::time_duration &interval, Delegate &delegate, Method method) noexcept;

    /** \brief cancels previously started timer
     *
     * If timer hasn't been triggered, then it is cancelled and the callback will be invoked
     * with `true` to mark that it was cancelled.
     *
     * Upon cancellation the timer callback will be invoked immediately, in the context of caller.
     */
    void cancel_timer(request_id_t request_id) noexcept;

    /** \brief flag to mark, that actor is already executing initialization */
    static const constexpr std::uint32_t PROGRESS_INIT = 1 << 0;

    /** \brief flag to mark, that actor is already executing shutdown */
    static const constexpr std::uint32_t PROGRESS_SHUTDOWN = 1 << 1;

  protected:
    /** \brief timer-id to timer-handler map (type) */
    using timers_map_t = std::unordered_map<request_id_t, timer_handler_ptr_t>;

    /** \brief triggers timer handler associated with the timer id */
    void on_timer_trigger(request_id_t request_id, bool cancelled) noexcept;

    /** \brief starts timer with pre-forged timer id (aka request-id */
    template <typename Delegate, typename Method>
    void start_timer(request_id_t request_id, const pt::time_duration &interval, Delegate &delegate,
                     Method method) noexcept;

    void assign_shutdown_reason(extended_error_ptr_t reason) noexcept;

    /** \brief suspended init request message */
    intrusive_ptr_t<message::init_request_t> init_request;

    /** \brief suspended shutdown request message */
    intrusive_ptr_t<message::shutdown_request_t> shutdown_request;

    /** \brief actor address */
    address_ptr_t address;

    /** \brief actor identity, wich might have some meaning for developers */
    std::string identity;

    /** \brief non-owning pointer to actor's execution / infrastructure context */
    supervisor_t *supervisor;

    /** \brief opaque plugins storage (owning) */
    plugin_storage_ptr_t plugins_storage;

    /** \brief non-ownling list of plugins */
    plugins_t plugins;

    /** \brief timeout for actor initialization (used by supervisor) */
    pt::time_duration init_timeout;

    /** \brief timeout for actor shutdown (used by supervisorr) */
    pt::time_duration shutdown_timeout;

    /** \brief current actor state */
    state_t state;

    /** \brief non-owning pointer to address_maker plugin */
    plugin::address_maker_plugin_t *address_maker = nullptr;

    /** \brief non-owning pointer to lifetime plugin */
    plugin::lifetime_plugin_t *lifetime = nullptr;

    /** \brief non-owning pointer to link_server plugin */
    plugin::link_server_plugin_t *link_server = nullptr;

    /** \brief non-owning pointer to resources plugin */
    plugin::resources_plugin_t *resources = nullptr;

    /** \brief finds plugin by plugin class identity
     *
     * `nullptr` is returned when plugin cannot be found
     */
    plugin::plugin_base_t *get_plugin(const void *identity) const noexcept;

    /** \brief set of activating plugin identities */
    std::set<const void *> activating_plugins;

    /** \brief set of deactivating plugin identities */
    std::set<const void *> deactivating_plugins;

    /** \brief timer-id to timer-handler map */
    timers_map_t timers_map;

    /** \brief set of currently proccessing states, i.e. init or shutdown
     *
     * This is not the same as `state_t` flag, which just marks the state.
     *
     * The `continuation_mask` is mostly used by plugins to avoid recursion
     *
     */
    std::uint32_t continuation_mask = 0;

    extended_error_ptr_t shutdown_reason;

    friend struct plugin::plugin_base_t;
    friend struct plugin::lifetime_plugin_t;
    friend struct supervisor_t;
    template <typename T> friend struct request_builder_t;
    template <typename T, typename M> friend struct accessor_t;
};

} // namespace rotor
