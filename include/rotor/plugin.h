#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "messages.hpp"
#include "subscription.h"
#include <system_error>
#include <unordered_set>
#include <set>

namespace rotor {

enum class slot_t { INIT = 0, SHUTDOWN, SUBSCRIPTION, UNSUBSCRIPTION };


struct plugin_t {

    plugin_t() = default;
    virtual ~plugin_t();

    virtual bool is_complete_for(slot_t slot) noexcept;
    virtual bool is_complete_for(slot_t slot, const subscription_point_t& point) noexcept;
    virtual void activate(actor_base_t* actor) noexcept;
    virtual void deactivate() noexcept;

    template<typename Handler> handler_ptr_t subscribe(Handler&& handler, const address_ptr_t& address) noexcept;
    template<typename Handler> handler_ptr_t subscribe(Handler&& handler) noexcept;

    actor_base_t* actor;
    subscription_points_t own_subscriptions;
};



namespace internal {

struct subscription_plugin_t: public plugin_t {
    using plugin_t::plugin_t;
    using iterator_t = typename subscription_points_t::iterator;

    virtual void activate(actor_base_t* actor) noexcept override;
    virtual void deactivate() noexcept override;

    iterator_t find_subscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept;

    /* \brief unsubcribes all actor's handlers */
    void unsubscribe() noexcept;

    /** \brief recorded subscription points (i.e. handler/address pairs) */
    subscription_points_t points;

    virtual bool is_complete_for(slot_t slot, const subscription_point_t& point) noexcept override;
    virtual void on_subscription(message::subscription_t&) noexcept;
    virtual void on_unsubscription(message::unsubscription_t&) noexcept;
    virtual void on_unsubscription_external(message::unsubscription_external_t&) noexcept;
};

struct actor_lifetime_plugin_t: public plugin_t {
    using plugin_t::plugin_t;

    virtual void activate(actor_base_t* actor) noexcept override;
    virtual void deactivate() noexcept override;

    virtual address_ptr_t create_address() noexcept;
};

struct init_shutdown_plugin_t: public plugin_t {
    using plugin_t::plugin_t;

    virtual bool is_complete_for(slot_t slot) noexcept override;
    virtual void activate(actor_base_t* actor) noexcept override;
    virtual void deactivate() noexcept override;
    virtual void confirm_init() noexcept;
    virtual void confirm_shutdown() noexcept;

    virtual void on_init(message::init_request_t&) noexcept;
    virtual void on_shutdown(message::shutdown_request_t&) noexcept;

    /** \brief suspended init request message */
    intrusive_ptr_t<message::init_request_t> init_request;
};

struct initializer_plugin_t: public plugin_t {
    virtual void activate(actor_base_t* actor) noexcept override;
    virtual bool is_complete_for(slot_t slot) noexcept override;
    virtual bool is_complete_for(slot_t slot, const subscription_point_t& point) noexcept override;
    template<typename Handler> void subscribe_actor(Handler&& handler) noexcept;
    subscription_points_t tracked;
};


struct starter_plugin_t: public plugin_t {
    using plugin_t::plugin_t;

    virtual void activate(actor_base_t* actor) noexcept override;
    void on_start(message::start_trigger_t& message) noexcept;
};


// supervisor plugins

struct locality_plugin_t: public plugin_t {
    using plugin_t::plugin_t;

    virtual void activate(actor_base_t* actor) noexcept override;
};

struct subscription_support_plugin_t: public plugin_t {
    using plugin_t::plugin_t;

    virtual void activate(actor_base_t* actor) noexcept override;
    virtual void deactivate() noexcept override;
    virtual void on_call(message::handler_call_t& message) noexcept;
    virtual void on_unsubscription(message::commit_unsubscription_t& message) noexcept;
    virtual void on_unsubscription_external(message::external_subscription_t& message) noexcept;
};

struct children_manager_plugin_t: public plugin_t {
    using plugin_t::plugin_t;


    /** \brief child actror housekeeping structure */
    struct actor_state_t {
        /** \brief intrusive pointer to actor */
        actor_ptr_t actor;

        /** \brief whethe the shutdown request is already sent */
        bool shutdown_requesting;
    };

    /** \brief (local) address-to-child_actor map type */
    using actors_map_t = std::unordered_map<address_ptr_t, actor_state_t>;

    /** \brief type for keeping list of initializing actors (during supervisor inititalization) */
    using initializing_actors_t = std::unordered_set<address_ptr_t>;

    virtual void activate(actor_base_t* actor) noexcept override;
    virtual void deactivate() noexcept override;
    virtual bool is_complete_for(slot_t slot) noexcept override;

    virtual void create_child(const actor_ptr_t &actor) noexcept;
    virtual void remove_child(actor_base_t &actor) noexcept;
    virtual void on_shutdown_fail(actor_base_t &actor, const std::error_code &ec) noexcept;

    virtual void on_create(message::create_actor_t& message) noexcept;
    virtual void on_init(message::init_response_t& message) noexcept;
    virtual void on_shutdown_trigger(message::shutdown_trigger_t& message) noexcept;
    virtual void on_shutdown_confirm(message::shutdown_response_t& message) noexcept;

    bool postponed_init = false;
    bool activated = false;
    /** \brief local address to local actor (intrusive pointer) mapping */
    actors_map_t actors_map;

    /** \brief list of initializing actors (during supervisor inititalization) */
    initializing_actors_t initializing_actors;
};

} // namespace internal

} // namespace rotor
