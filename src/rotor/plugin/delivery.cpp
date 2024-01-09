//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/delivery.h"
#include "rotor/supervisor.h"
#include "rotor/messages.hpp"
#include <boost/core/demangle.hpp>
#include <iostream>
#include <cstdlib>
#include <sstream>

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct main_address {};
struct context {};
} // namespace to
} // namespace

template <> auto &subscription_t::access<to::main_address>() noexcept { return main_address; }
template <> auto &supervisor_t::access<to::context>() noexcept { return context; }

void delivery_plugin_base_t::activate(actor_base_t *actor_) noexcept {
    plugin_base_t::activate(actor_);
    auto sup = static_cast<supervisor_t *>(actor_);
    queue = &sup->locality_leader->queue;
    address = sup->address.get();
    subscription_map = &sup->subscription_map;
    subscription_map->access<to::main_address>() = address;
    sup->delivery = this;
    stringifier = &actor->get_supervisor().access<to::context>()->get_stringifier();
}

void local_delivery_t::delivery(message_ptr_t &message,
                                const subscription_t::joint_handlers_t &local_recipients) noexcept {
    for (auto &handler : local_recipients.external) {
        auto &sup = handler->actor_ptr->get_supervisor();
        auto &address = sup.get_address();
        auto wrapped_message = make_message<payload::handler_call_t>(address, message, handler);
        sup.enqueue(std::move(wrapped_message));
    }
    for (auto &handler : local_recipients.internal) {
        handler->call(message);
    }
}

static int get_message_level(const message_base_t *message) noexcept {
    auto type = message->type_index;
    if (type == message::unsubscription_t::message_type) {
        return 9;
    } else if (type == message::unsubscription_t::message_type) {
        return 9;
    } else if (type == message::subscription_t::message_type) {
        return 9;
    } else if (type == message::unsubscription_external_t::message_type) {
        return 9;
    } else if (type == message::external_subscription_t::message_type) {
        return 9;
    } else if (type == message::commit_unsubscription_t::message_type) {
        return 9;
    } else if (type == message::deregistration_service_t::message_type) {
        return 2;
    } else if (type == message::registration_request_t::message_type) {
        return 2;
    } else if (type == message::registration_response_t::message_type) {
        return 2;
    } else if (type == message::discovery_request_t::message_type) {
        return 2;
    } else if (type == message::discovery_response_t::message_type) {
        return 2;
    } else if (type == message::discovery_promise_t::message_type) {
        return 2;
    } else if (type == message::discovery_future_t::message_type) {
        return 2;
    } else if (type == message::discovery_cancel_t::message_type) {
        return 2;
    } else if (type == message::link_request_t::message_type) {
        return 5;
    } else if (type == message::link_response_t::message_type) {
        return 3;
    } else if (type == message::shutdown_trigger_t::message_type) {
        return 1;
    } else if (type == message::handler_call_t::message_type) {
        return 20;
    } else if (type == message::init_request_t::message_type) {
        return 11;
    } else if (type == message::init_response_t::message_type) {
        return 11;
    } else if (type == message::start_trigger_t::message_type) {
        return 10;
    } else if (type == message::shutdown_request_t::message_type) {
        return 15;
    } else if (type == message::shutdown_response_t::message_type) {
        return 15;
    } else if (type == message::create_actor_t::message_type) {
        return 7;
    } else if (type == message::spawn_actor_t::message_type) {
        return 7;
    } else if (type == message::deregistration_notify_t::message_type) {
        return 3;
    } else if (type == message::unlink_notify_t::message_type) {
        return 8;
    } else if (type == message::unlink_request_t::message_type) {
        return 8;
    } else if (type == message::unlink_response_t::message_type) {
        return 8;
    }
    return 0;
}

static void dump_message(const char *prefix, const message_ptr_t &message,
                         const message_stringifier_t *stringifier) noexcept {
    auto var = std::getenv("ROTOR_INSPECT_DELIVERY");
    if (var) {
        int threshold = atoi(var);
        auto level = get_message_level(message.get());
        if (level <= threshold) {
            std::cout << prefix << "{" << level << "} " << stringifier->stringify(*message) << "\n";
        }
    }
}

void inspected_local_delivery_t::delivery(message_ptr_t &message,
                                          const subscription_t::joint_handlers_t &local_recipients,
                                          const message_stringifier_t *stringifier) noexcept {
    dump_message(">> ", message, stringifier);
    local_delivery_t::delivery(message, local_recipients);
}

void inspected_local_delivery_t::discard(message_ptr_t &message, const message_stringifier_t *stringifier) noexcept {
    dump_message("<DISCARDED> ", message, stringifier);
}
