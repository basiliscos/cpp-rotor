#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "message.h"
#include "state.h"

namespace rotor {

struct handler_base_t;
using actor_ptr_t = intrusive_ptr_t<actor_base_t>;
using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

namespace payload {

struct initialize_actor_t {
    address_ptr_t actor_address;
};
struct initialize_confirmation_t {
    address_ptr_t actor_address;
};
struct start_actor_t {
    address_ptr_t actor_address;
};

struct create_actor_t {
    actor_ptr_t actor;
};

struct shutdown_request_t {
    address_ptr_t actor_address;
};

struct shutdown_confirmation_t {
    address_ptr_t actor_address;
};

struct handler_call_t {
    message_ptr_t orig_message;
    handler_ptr_t handler;
};

struct external_subscription_t {
    address_ptr_t addr;
    handler_ptr_t handler;
};

struct subscription_confirmation_t {
    address_ptr_t addr;
    handler_ptr_t handler;
};

struct external_unsubscription_t {
    address_ptr_t addr;
    handler_ptr_t handler;
};

struct commit_unsubscription_t {
    address_ptr_t addr;
    handler_ptr_t handler;
};

struct unsubscription_confirmation_t {
    address_ptr_t addr;
    handler_ptr_t handler;
};

struct state_request_t {
    address_ptr_t reply_addr;
    address_ptr_t subject_addr;
};

struct state_response_t {
    address_ptr_t subject_addr;
    state_t state;
};

} // namespace payload

} // namespace rotor
