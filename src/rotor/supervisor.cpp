//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include <assert.h>
#include <iostream>
//#include <boost/core/demangle.hpp>

using namespace rotor;

supervisor_t::supervisor_t(supervisor_t *sup) : actor_base_t(*this), parent{sup} {}

address_ptr_t supervisor_t::make_address() noexcept { return new address_t{*this}; }

void supervisor_t::do_initialize(system_context_t *ctx) noexcept {
    context = ctx;
    actor_base_t::do_initialize(ctx);
    subscribe(&supervisor_t::on_call);
    subscribe(&supervisor_t::on_initialize_confirm);
    subscribe(&supervisor_t::on_shutdown_confirm);
    subscribe(&supervisor_t::on_create);
    subscribe(&supervisor_t::on_external_subs);
    subscribe(&supervisor_t::on_commit_unsubscription);
    subscribe(&supervisor_t::on_state_request);
    auto addr = supervisor.get_address();
    state = state_t::INITIALIZED;
    send<payload::initialize_actor_t>(addr, addr);
}

void supervisor_t::on_create(message_t<payload::create_actor_t> &msg) noexcept {
    auto actor = msg.payload.actor;
    auto actor_address = actor->get_address();
    actors_map.emplace(actor_address, std::move(actor));
    send<payload::initialize_actor_t>(address, actor_address);
}

void supervisor_t::on_initialize_confirm(message_t<payload::initialize_confirmation_t> &msg) noexcept {
    auto &addr = msg.payload.actor_address;
    send<payload::start_actor_t>(addr, addr);
}

void supervisor_t::do_start() noexcept { send<payload::start_actor_t>(address); }

void supervisor_t::do_process() noexcept {
    while (outbound.size()) {
        auto message = outbound.front();
        auto &dest = message->address;
        outbound.pop_front();
        auto internal = &dest->supervisor == this;
        // std::cout << "msg [" << (internal ? "i" : "e") << "] :" << boost::core::demangle((const char*)
        // message->get_type_index()) << "\n";
        if (internal) { /* subscriptions are handled by me */
            /*
            if (state == state_t::SHUTTED_DOWN)
                continue;
            */
            auto it_subscriptions = subscription_map.find(dest);
            if (it_subscriptions != subscription_map.end()) {
                auto &subscription = it_subscriptions->second;
                auto recipients = subscription.get_recipients(message->get_type_index());
                if (recipients) {
                    for (auto &it : *recipients) {
                        if (it.mine) {
                            it.handler->call(message);
                        } else {
                            auto &sup = it.handler->supervisor;
                            auto wrapped_message =
                                make_message<payload::handler_call_t>(sup->address, message, it.handler);
                            sup->enqueue(std::move(wrapped_message));
                        }
                    }
                }
            }
        } else {
            dest->supervisor.enqueue(std::move(message));
        }
    }
}

size_t supervisor_t::unsubscribe_actor(const actor_ptr_t &actor) noexcept {
    auto &points = actor->get_subscription_points();
    auto count = points.size();
    /* TODO: unsubscribe backwards */
    auto it = points.rbegin();
    while (it != points.rend()) {
        auto &addr = it->address;
        auto &handler = it->handler;
        unsubscribe_actor(addr, handler);
        ++it;
    }
    return count;
}

void supervisor_t::on_initialize(message_t<payload::initialize_actor_t> &msg) noexcept {
    auto actor_addr = msg.payload.actor_address;
    if (actor_addr == address) {
        state = state_t::OPERATIONAL;
    } else {
        send<payload::initialize_actor_t>(actor_addr, actor_addr);
    }
}

void supervisor_t::on_shutdown(message_t<payload::shutdown_request_t> &msg) noexcept {
    auto &source_addr = msg.payload.actor_address;
    if (source_addr == address) {
        actor_ptr_t self{this};
        state = state_t::SHUTTING_DOWN;
        for (auto pair : actors_map) {
            auto &addr = pair.first;
            send<payload::shutdown_request_t>(addr, addr);
        }
        if (!actors_map.empty()) {
            start_shutdown_timer();
        } else {
            actor_ptr_t self{this};
            unsubscribe_actor(self);
        }
    } else {
        send<payload::shutdown_request_t>(source_addr, source_addr);
    }
}

void supervisor_t::on_shutdown_timer_trigger() noexcept {
    auto err = make_error_code(error_code_t::shutdown_timeout);
    context->on_error(err);
}

void supervisor_t::on_shutdown_confirm(message_t<payload::shutdown_confirmation_t> &message) noexcept {
    // std::cout << "supervisor_t::on_shutdown_confirm\n";
    auto &source_addr = message.payload.actor_address;
    if (source_addr != address) {
        auto &actor = actors_map.at(source_addr);
        remove_actor(*actor);
    }
    if (actors_map.empty() && state == state_t::SHUTTING_DOWN) {
        actor_ptr_t self{this};
        unsubscribe_actor(self);
    }
}

void supervisor_t::on_external_subs(message_t<payload::external_subscription_t> &message) noexcept {
    auto &handler = message.payload.handler;
    auto &addr = message.payload.addr;
    assert(&addr->supervisor == this);
    subscribe_actor(addr, handler);
}

void supervisor_t::on_commit_unsubscription(message_t<payload::commit_unsubscription_t> &message) noexcept {
    commit_unsubscription(message.payload.addr, message.payload.handler);
}

void supervisor_t::on_call(message_t<payload::handler_call_t> &message) noexcept {
    auto &handler = message.payload.handler;
    auto &orig_message = message.payload.orig_message;
    handler->call(orig_message);
}

void supervisor_t::on_state_request(message_t<payload::state_request_t> &message) noexcept {
    auto &addr = message.payload.subject_addr;
    auto &reply_to = message.payload.reply_addr;
    state_t target_state = state_t::UNKNOWN;
    if (addr == address) {
        target_state = state;
    } else {
        auto it = actors_map.find(addr);
        if (it != actors_map.end()) {
            target_state = it->second->state;
        }
    }
    send<payload::state_response_t>(reply_to, addr, target_state);
}

void supervisor_t::commit_unsubscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept {
    auto &subscriptions = subscription_map.at(addr);
    auto left = subscriptions.unsubscribe(handler);
    if (!left) {
        subscription_map.erase(addr);
    }
}

void supervisor_t::remove_actor(actor_base_t &actor) noexcept {
    auto it_actor = actors_map.find(actor.address);
    if (it_actor == actors_map.end()) {
        context->on_error(make_error_code(error_code_t::missing_actor));
    }
    actors_map.erase(it_actor);
    if (actors_map.empty() && state == state_t::SHUTTING_DOWN) {
        cancel_shutdown_timer();
        actor_ptr_t self{this};
        unsubscribe_actor(self);
    }
}

void supervisor_t::confirm_shutdown() noexcept {
    if (parent) {
        send<payload::shutdown_confirmation_t>(parent->get_address(), address);
    }
}
