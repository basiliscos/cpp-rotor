#pragma once
//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "actor_base.h"
#include "messages.hpp"
#include <unordered_map>
#include <set>
#include <list>

namespace rotor {

/** \struct registry_t
 *  \brief keeps name-to-service_address mapping at runtime
 *
 *  The class solves the following problem: one actor needs to know
 *  somehow the address of other actor. Instead of tightly couple
 *  them, i.e. manually binding addresses, it is possible to just
 *  take the registry everywhere and ask it (or registere there)
 *  just on actor startup (initialization)
 *
 *  The "server-actor" must register it's address first. The name
 *  must be unique (in the scope of registry); the same address
 *  can be registered on different names, if need. It is good
 *  practice to unregister addresses when actor is going to be
 *  shutdown (it is done by `registry_plugin_t`).
 *  It is possible to unregister single name, or all
 *  names associated with an address.
 *
 *  The discovery response returned to "client-actor" might contain
 *  error code if there is no address associated with the asked name.
 *
 *  The actor has syncornization facilities, i.e. it is possible to
 *  ask the actor to notify when some name has been registered
 * (discovery promise/future).
 *
 */
struct registry_t : public actor_base_t {
    using actor_base_t::actor_base_t;

    void configure(plugin::plugin_base_t &plugin) noexcept override;

    /** \brief registers address on the service. Fails, if the name already exists */
    virtual void on_reg(message::registration_request_t &request) noexcept;

    /** \brief deregisters the name in the registry */
    virtual void on_dereg_service(message::deregistration_service_t &message) noexcept;

    /** \brief deregisters all names assosiccated with the service address */
    virtual void on_dereg(message::deregistration_notify_t &message) noexcept;

    /** \brief returns service address associated with the name or error */
    virtual void on_discovery(message::discovery_request_t &request) noexcept;

    /** \brief notify the requestee when service name/address becomes available */
    virtual void on_promise(message::discovery_promise_t &request) noexcept;

    /** \brief cancels previously asked discovery promise */
    virtual void on_cancel(message::discovery_cancel_t &notify) noexcept;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

  protected:
    /** \brief intrusive pointer to discovery promise message (type) */
    using promise_ptr_t = intrusive_ptr_t<message::discovery_promise_t>;

    /** \brief list of intrusive pointers to discovery promise messages (type) */
    using promises_list_t = std::list<promise_ptr_t>;

    /** \brief name-to-address mapping type */
    using registered_map_t = std::unordered_map<std::string, address_ptr_t>;

    /** \brief set of registered names type (for single address) */
    using registered_names_t = std::set<std::string>;

    /** \brief service address to registered names mapping type */
    using revese_map_t = std::unordered_map<address_ptr_t, registered_names_t>;

    /** \brief service-name to list of promises map (type)*/
    using promises_map_t = std::unordered_map<std::string, promises_list_t>;

    /** \brief name-to-address mapping */
    registered_map_t registered_map;

    /** \brief address-to-list_of_names mapping */
    revese_map_t revese_map;

    /** \brief service-name to list of promises map */
    promises_map_t promises_map;
};

} // namespace rotor
