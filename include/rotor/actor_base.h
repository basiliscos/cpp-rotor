#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "actor_config.h"
#include "messages.hpp"
#include "state.h"
#include "handler.hpp"
#include "forward.hpp"
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
 * as supervirors without any need of synchonization.
 *
 * All actor methods are thread-unsafe, i.e. should not be called with except of
 * it's own supervisor. Communication with actor should be performed via messages.
 *
 * Actor is addressed by it's "main" address; however it is possible for an actor
 * to have multiple identities aka "virtual" addresses.
 *
 */
struct actor_base_t : public arc_base_t<actor_base_t> {
    using config_t = actor_config_t;
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
    using plugins_list_t = std::tuple<
        plugin::address_maker_plugin_t,
        plugin::lifetime_plugin_t,
        plugin::init_shutdown_plugin_t,
        plugin::prestarter_plugin_t,
        plugin::link_server_plugin_t,
        plugin::link_client_plugin_t,
        plugin::registry_plugin_t,
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
     * Actor's "main" address is created, actor's behavior is created.
     *
     * Actor performs subsscription on all major methods, defined by
     * `rotor` framework; sets internal actor state to `INITIALIZING`.
     *
     */
    virtual void do_initialize(system_context_t *ctx) noexcept;

    /** \brief convenient method to send actor's supervisor shutdown trigger message */
    virtual void do_shutdown() noexcept;

    virtual void unsubscribe() noexcept;

    virtual void on_start() noexcept;

    /** \brief sends message to the destination address
     *
     * Internally it just constructs new message in supervisor's outbound queue.
     *
     */
    template <typename M, typename... Args> void send(const address_ptr_t &addr, Args &&... args);

    /** \brief returns request builder for destination address using the "main" actor address
     *
     * The `args` are forwarded for construction of the request. The request is not actually sent,
     * until `send` method of {@link request_builder_t} will be invoked.
     *
     * Supervisor will spawn timeout timer upon `timeout` method.
     */
    template <typename R, typename... Args>
    request_builder_t<typename request_wrapper_t<R>::request_t> request(const address_ptr_t &dest_addr,
                                                                        Args &&... args);

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
    request_via(const address_ptr_t &dest_addr, const address_ptr_t &reply_addr, Args &&... args);

    /** \brief convenient method for constructing and sending response to a request
     *
     * `args` are forwarded to response payload constuction
     */
    template <typename Request, typename... Args> void reply_to(Request &message, Args &&... args);

    /** \brief convenient method for constructing and sending error response to a request */
    template <typename Request, typename... Args> void reply_with_error(Request &message, const std::error_code &ec);

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

    void activate_plugins() noexcept;
    void commit_plugin_activation(plugin::plugin_base_t &plugin, bool success) noexcept;

    void deactivate_plugins() noexcept;
    void commit_plugin_deactivation(plugin::plugin_base_t &plugin) noexcept;

    void on_subscription(message::subscription_t &message) noexcept;
    void on_unsubscription(message::unsubscription_t &message) noexcept;
    void on_unsubscription_external(message::unsubscription_external_t &message) noexcept;

    address_ptr_t create_address() noexcept;

    /** \brief finalize shutdown, release aquired resources
     *
     * This is the last action in the shutdown sequence.
     * No further methods will be invoked.
     *
     */
    virtual void shutdown_finish() noexcept;
    virtual void shutdown_start() noexcept;

    /* brief strart releasing acquired resources
     *
     * The method can be overriden in derived classes to initiate the
     * release of resources, i.e. (asynchronously) close all opened
     * sockets before confirm shutdown to supervisor.
     *
     * It actually forwards shutdown for the behavior
     *
     */
    void shutdown_continue() noexcept;

    /** \brief finializes initialization  */
    virtual void init_start() noexcept;

    /*  starts initialization
     *
     * Some resources might be acquired synchronously, if needed. If resources need
     * to be acquired asynchronously this method should be overriden, and
     * invoked only after resources acquisition.
     *
     * In internals it forwards initialization sequence to the behavior.
     *
     */
    void init_continue() noexcept;

    /** \brief finializes initialization  */
    virtual void init_finish() noexcept;

    virtual void configure(plugin::plugin_base_t &plugin) noexcept;

    template <typename T> auto &access() noexcept;
    template <typename T, typename... Args> auto access(Args... args) noexcept;
    template <typename T> auto &access() const noexcept;
    template <typename T, typename... Args> auto access(Args... args) const noexcept;

    inline const address_ptr_t &get_address() const noexcept { return address; }

    inline supervisor_t &get_supervisor() noexcept { return *supervisor; }

  protected:
    virtual bool ready_to_shutdown() noexcept;

    /** \brief suspended init request message */
    intrusive_ptr_t<message::init_request_t> init_request;

    /** \brief suspended shutdown request message */
    intrusive_ptr_t<message::shutdown_request_t> shutdown_request;

    /** \brief actor address */
    address_ptr_t address;

    /** \brief non-owning pointer to actor's execution / infrastructure context */
    supervisor_t *supervisor;

    plugin_storage_ptr_t plugins_storage;
    plugins_t plugins;

    pt::time_duration init_timeout;
    pt::time_duration shutdown_timeout;

    /** \brief current actor state */
    state_t state;

    /* non-owning pointers */
    plugin::address_maker_plugin_t *address_maker = nullptr;
    plugin::lifetime_plugin_t *lifetime = nullptr;

    plugin::plugin_base_t *get_plugin(const void *identity) const noexcept;

    std::set<const void *> activating_plugins;
    std::set<const void *> deactivating_plugins;

    friend struct plugin::plugin_base_t;
    friend struct plugin::lifetime_plugin_t;
    friend struct supervisor_t;
    template <typename T> friend struct request_builder_t;
    template <typename T, typename M> friend struct accessor_t;
};

} // namespace rotor
