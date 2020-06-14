//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//


#include "rotor/supervisor.h"
#include <assert.h>
/*
#include <iostream>
#include <boost/core/demangle.hpp>
*/
using namespace rotor;

supervisor_t::supervisor_t(supervisor_config_t &config)
    : actor_base_t(config), parent{config.supervisor}, last_req_id{1},
      /* shutdown_timeout{config.shutdown_timeout},*/ policy{config.policy}, manager{nullptr}, subscription_support{nullptr},
      subscription_map(*this) {
    if (!supervisor) {
        supervisor = this;
    }
    supervisor = this;
}

address_ptr_t supervisor_t::make_address() noexcept {
    auto root_sup = this;
    while (root_sup->parent) {
        root_sup = root_sup->parent;
    }
    return instantiate_address(root_sup);
}

address_ptr_t supervisor_t::instantiate_address(const void *locality) noexcept {
    return new address_t{*this, locality};
}

void supervisor_t::do_initialize(system_context_t *ctx) noexcept {
    context = ctx;
    actor_base_t::do_initialize(ctx);
    // do self-bootstrap
    if (!parent) {
        request<payload::initialize_actor_t>(address, address).send(shutdown_timeout);
    }
}

void supervisor_t::do_process() noexcept {
    auto effective_queue = &locality_leader->queue;
    while (effective_queue->size()) {
        auto message = effective_queue->front();
        auto &dest = message->address;
        effective_queue->pop_front();
        auto &dest_sup = dest->supervisor;
        auto internal = &dest_sup == this;
/*
        std::cout << "msg [" << (internal ? "i" : "e") << "] :" << boost::core::demangle((const char*)
             message->type_index) << " for " << dest.get() << "\n";
*/
        if (internal) { /* subscriptions are handled by me */
            deliver_local(std::move(message));
        } else if (dest_sup.address->same_locality(*address)) {
            dest_sup.deliver_local(std::move(message));
        } else {
            dest_sup.enqueue(std::move(message));
        }
    }
}

void supervisor_t::deliver_local(message_ptr_t &&message) noexcept {
    auto local_recipients = subscription_map.get_recipients(*message);
    if (local_recipients) {
        // external should be handled 1st, as the subscription might be deleted during internal handler processing.
        for(auto handler: local_recipients->external) {
            auto &sup = handler->actor_ptr->get_supervisor();
            auto wrapped_message = make_message<payload::handler_call_t>(sup.address, message, handler);
            sup.enqueue(std::move(wrapped_message));
        }
        for(auto handler: local_recipients->internal) {
            handler->call(message);
        }
    }
}

void supervisor_t::do_shutdown() noexcept {
    auto upstream_sup = parent ? parent : this;
    send<payload::shutdown_trigger_t>(upstream_sup->get_address(), address);
}


subscription_info_ptr_t supervisor_t::subscribe_actor(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept {
   auto sub_info = subscription_map.materialize(handler, addr);
   subscription_point_t point{handler, addr};

   if (sub_info->internal_address) {
       send<payload::subscription_confirmation_t>(handler->actor_ptr->get_address(), point);
   } else {
       send<payload::external_subscription_t>(addr->supervisor.address, point);
   }

   if (sub_info->internal_handler) {
       handler->actor_ptr->lifetime->initate_subscription(sub_info);
   }

   return sub_info;
}

void supervisor_t::commit_unsubscription(const subscription_info_ptr_t& info) noexcept {
    subscription_map.forget(info);
}

void supervisor_t::shutdown_finish() noexcept {
    address_mapping.destructive_get(*this);
    actor_base_t::shutdown_finish();
}

void supervisor_t::on_timer_trigger(timer_id_t timer_id) {
    auto it = request_map.find(timer_id);
    if (it != request_map.end()) {
        auto &request_curry = it->second;
        message_ptr_t &request = request_curry.request_message;
        auto ec = make_error_code(error_code_t::request_timeout);
        auto timeout_message = request_curry.fn(request_curry.reply_to, *request, std::move(ec));
        put(std::move(timeout_message));
        request_map.erase(it);
    }
}
