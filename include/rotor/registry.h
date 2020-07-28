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
 *  shutdown. It is possible to unregister single name, or all
 *  names associated with an address.
 *
 *  The discovery response returned to "client-actor" might contain
 *  error code if there is no address associated with the asked name.
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

  protected:
    /** \brief name-to-address mapping type */
    using registered_map_t = std::unordered_map<std::string, address_ptr_t>;

    /** \brief set of registered names type (for single address) */
    using registered_names_t = std::set<std::string>;

    /** \brief service address to registered names mapping type */
    using revese_map_t = std::unordered_map<address_ptr_t, registered_names_t>;

    /** \brief name-to-address mapping */
    registered_map_t registered_map;

    /** \brief address-to-list_of_names mapping */
    revese_map_t revese_map;
};

} // namespace rotor
