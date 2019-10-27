#pragma once
//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "actor_base.h"
#include "messages.hpp"
#include <unordered_map>
#include <set>

namespace rotor {

struct registry_t : public actor_base_t {
    using actor_base_t::actor_base_t;

    virtual ~registry_t();

    virtual void init_start() noexcept override;

    virtual void on_reg(message::registration_request_t &request) noexcept;
    virtual void on_dereg_service(message::deregistration_service_t &message) noexcept;
    virtual void on_dereg(message::deregistration_notify_t &message) noexcept;
    virtual void on_discovery(message::discovery_request_t &request) noexcept;

    using registered_map_t = std::unordered_map<std::string, address_ptr_t>;
    using registered_names_t = std::set<std::string>;
    using revese_map_t = std::unordered_map<address_ptr_t, registered_names_t>;

    registered_map_t registered_map;
    revese_map_t revese_map;
};

} // namespace rotor
