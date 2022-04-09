#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "../subscription.h"
#include "rotor/messages.hpp"

namespace rotor::plugin {

enum class config_phase_t { PREINIT = 0b01, INITIALIZING = 0b10 };

/** \struct plugin_base_t
 *
 * \brief base class for all actor plugins
 *
 */
struct ROTOR_API plugin_base_t {
    /** \brief possible plugin's reactions on actor lifetime events */
    enum reaction_t {
        INIT = 1 << 0,
        SHUTDOWN = 1 << 1,
        SUBSCRIPTION = 1 << 2,
        START = 1 << 3,
    };

    /** \brief default plugin ctor */
    plugin_base_t() = default;

    /** \brief copy ctor is not allowed */
    plugin_base_t(const plugin_base_t &) = delete;
    virtual ~plugin_base_t() = default;

    /** \brief returns pointer, which uniquely identifes plugin type */
    virtual const void *identity() const noexcept = 0;

    /** \brief invoked by actor upon initialization.
     *
     * It it responsible for setting up reactions and further installation, e.g.
     * messages subscriptions and may calling back actor's configuration
     * (which can be postponed).
     *
     * Before the activate is invoked, the plugin is in `PREINIT` phase,
     * after the activate is called, the plugin is in `INITIALIZING` phase.
     * This might be useful for early/late (double) plugin configuration
     * in actor.
     */
    virtual void activate(actor_base_t *actor) noexcept;

    /** \brief deactivates plugin from an actor
     *
     * The method can be intercepted by descendants to do not deactivate
     * too early, and called when needed.
     *
     * The `plugin_base_t` implementation unsubscribes from all plugin
     * subscriptions and when no subscription left commits deactivation
     * to an actor.
     *
     */
    virtual void deactivate() noexcept;

    /** \brief polls plugin, whether it is done with initialization
     *
     * The method is invoked only when `init` reaction was set on.
     *
     * The `true` is returned, that means plugin is done with init and
     * will not be longer polled in future.
     *
     */
    virtual bool handle_init(message::init_request_t *message) noexcept;

    /** \brief polls plugin, whether it is done with shutdown
     *
     * The method is invoked only when `shutdown` reaction was set on.
     *
     * The `true` is returned, that means plugin is done with shutdown and
     * will not be longer polled in future.
     *
     */
    virtual bool handle_shutdown(message::shutdown_request_t *message) noexcept;

    /** \brief polls plugin, whether it is done with start
     *
     * The method is invoked only when `start` reaction was set on.
     *
     * The `true` is returned, that means plugin is done with start and
     * will not be longer polled in future.
     *
     */
    virtual void handle_start(message::start_trigger_t *message) noexcept;

    /** \brief polls plugin, whether it is done with subscription
     *
     * The method is invoked only when `subscription` reaction was set on.
     *
     * The `true` is returned, that means plugin is done with start and
     * will not be longer polled in future.
     *
     */
    virtual bool handle_subscription(message::subscription_t &message) noexcept;

    /** \brief polls plugin, whether it is done with unsubscription
     *
     * All plugins are polled for unsubscription, no special handling is
     * needed unless you know what your are doing.
     *
     * The `true` is returned, that means plugin is done with unsubscription and
     * will not be longer polled in future. The default is `false`.
     *
     */
    virtual bool handle_unsubscription(const subscription_point_t &point, bool external) noexcept;

    /** \brief remove subscription point from internal storage
     *
     * If there is no any subscription left, the plugin is deactivated
     */
    virtual bool forget_subscription(const subscription_point_t &point) noexcept;

    /** \brief commits unsubscription via supervisor
     *
     * Used for foreign subscriptions.
     *
     */
    virtual void forget_subscription(const subscription_info_ptr_t &info) noexcept;

    /** \brief invokes the callback if plugin type and phase mach
     *
     * Upon successful match plugin is casted to the final type, and only then
     * passed by reference to the callback. Used in the generic actors plugin
     * confuguration to inject custom (per-plugin-type) configuration.
     *
     */
    template <typename Plugin, typename Fn>
    void with_casted(Fn &&fn, config_phase_t desired_phase = config_phase_t::INITIALIZING) noexcept {
        int phase_match = static_cast<int>(phase) & static_cast<int>(desired_phase);
        if (phase_match && identity() == Plugin::class_identity) {
            auto &final = static_cast<Plugin &>(*this);
            fn(final);
        }
    }

    /** \brief returns the current set of plugin reactions */
    std::size_t get_reaction() const noexcept { return reaction; }

    /** \brief turns on the specified reaction of the plugin */
    void reaction_on(reaction_t value) noexcept { reaction = reaction | value; }

    /** \brief turns off the specified reaction of the plugin */
    void reaction_off(reaction_t value) noexcept { reaction = reaction & ~value; }

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

  protected:
    /** \brief subscribes plugin to the custom *plugin* handler on the specified address */
    template <typename Handler>
    subscription_info_ptr_t subscribe(Handler &&handler, const address_ptr_t &address) noexcept;

    /** \brief subscribes plugin to the custom *plugin* handler on the main actor address */
    template <typename Handler> subscription_info_ptr_t subscribe(Handler &&handler) noexcept;

    /** \brief non-owning actor pointer
     *
     * It is non-null only after plugin activation.
     *
     */
    actor_base_t *actor;

    /** \brief makes an error within the context of actor */
    extended_error_ptr_t make_error(const std::error_code &ec, const extended_error_ptr_t &next = {}) noexcept;

  private:
    subscription_container_t own_subscriptions;
    std::size_t reaction = 0;
    config_phase_t phase = config_phase_t::PREINIT;
};

} // namespace rotor::plugin
