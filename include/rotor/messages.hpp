#pragma once
#include "address.hpp"
#include "message.h"

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
    actor_ptr_t actor;
};
struct shutdown_confirmation_t {
    actor_ptr_t actor;
};

struct handler_call_t {
    message_ptr_t orig_message;
    handler_ptr_t handler;
};

struct external_subscription_t {
    address_ptr_t addr;
    handler_ptr_t handler;
};

struct external_unsubscription_t {
    address_ptr_t addr;
    handler_ptr_t handler;
};

} // namespace payload

} // namespace rotor
