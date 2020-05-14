//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/children_manager.h"
#include "rotor/supervisor.h"
//#include <iostream>

using namespace rotor;
using namespace rotor::internal;

const void* children_manager_plugin_t::class_identity = static_cast<const void *>(typeid(children_manager_plugin_t).name());

const void* children_manager_plugin_t::identity() const noexcept {
    return class_identity;
}

bool children_manager_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    static_cast<supervisor_t&>(*actor_).manager = this;
    subscribe(&children_manager_plugin_t::on_create);
    subscribe(&children_manager_plugin_t::on_init);
    subscribe(&children_manager_plugin_t::on_shutdown_trigger);
    subscribe(&children_manager_plugin_t::on_shutdown_confirm);
    actor->install_plugin(*this, slot_t::INIT);
    actor->install_plugin(*this, slot_t::SHUTDOWN);
    actors_map.emplace(actor->get_address(), actor_state_t{actor, false});
    return false;
}


bool children_manager_plugin_t::deactivate() noexcept {
    assert(actors_map.size() == 1);
    actors_map.clear();
    initializing_actors.clear();
    return plugin_t::deactivate();
}

void children_manager_plugin_t::remove_child(actor_base_t &child) noexcept {
    auto it_actor = actors_map.find(child.address);
    assert(it_actor != actors_map.end());
    actors_map.erase(it_actor);
    if (actors_map.size() == 1 && actor->get_state() == state_t::SHUTTING_DOWN) {
        actor->shutdown_continue();
    }
}

void children_manager_plugin_t::create_child(const actor_ptr_t& child) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    child->do_initialize(sup.get_context());
    auto &timeout = child->get_init_timeout();
    sup.send<payload::create_actor_t>(sup.get_address(), child, timeout);
    if (!activated) { // sup.state == state_t::INITIALIZING
        initializing_actors.emplace(child->get_address());
        postponed_init = true;
    }
}

void children_manager_plugin_t::on_create(message::create_actor_t &message) noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto actor = message.payload.actor;
    auto actor_address = actor->get_address();
    actors_map.emplace(actor_address, actor_state_t{std::move(actor), false});
    sup.template request<payload::initialize_actor_t>(actor_address, actor_address).send(message.payload.timeout);
}

void children_manager_plugin_t::on_init(message::init_response_t &message) noexcept {
    auto &address = message.payload.req->payload.request_payload.actor_address;
    auto &ec = message.payload.ec;

    auto &sup = static_cast<supervisor_t &>(*actor);
    bool continue_init = false;
    auto it = initializing_actors.find(address);
    if (it != initializing_actors.end()) {
        initializing_actors.erase(it);
        // actor->get_state() == state_t::INITIALIZING;
    }
    continue_init = !activated && initializing_actors.empty() && !ec;
    if (ec) {
        auto shutdown_self = !activated && sup.policy == supervisor_policy_t::shutdown_self;
        if (shutdown_self) {
            continue_init = false;
            sup.do_shutdown();
        } else {
            sup.template request<payload::shutdown_request_t>(address, address).send(sup.shutdown_timeout);
        }
    } else {
        sup.template send<payload::start_actor_t>(address, address);
    }
    if (continue_init) {
        activated = true;
        if (postponed_init) {
            actor->init_continue();
        }
        plugin_t::activate(actor);
    }
}

void children_manager_plugin_t::on_shutdown_trigger(message::shutdown_trigger_t& message) noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto &source_addr = message.payload.actor_address;
    if (source_addr == sup.address) {
        if (sup.parent) {
            // will be routed via shutdown request
            sup.do_shutdown();
        } else {
            // do not do shutdown-request on self
            assert((actor->state != state_t::SHUTTING_DOWN) || (actor->state != state_t::SHUTTED_DOWN));
            actor->state = state_t::SHUTTING_DOWN;
            unsubscribe_all();
            actor->shutdown_continue();
        }
    } else {
        auto &actor_state = actors_map.at(source_addr);
        if (!actor_state.shutdown_requesting) {
            actor_state.shutdown_requesting = true;
            auto& timeout = actor->shutdown_timeout;
            sup.request<payload::shutdown_request_t>(source_addr, source_addr).send(timeout);
        }
    }
}

void children_manager_plugin_t::on_shutdown_fail(actor_base_t &actor, const std::error_code &ec) noexcept {
    actor.get_supervisor().get_context()->on_error(ec);
}


void children_manager_plugin_t::on_shutdown_confirm(message::shutdown_response_t& message) noexcept {
    auto &source_addr = message.payload.req->payload.request_payload.actor_address;
    auto &actor_state = actors_map.at(source_addr);
    actor_state.shutdown_requesting = false;
    auto &ec = message.payload.ec;
    auto child_actor = actor_state.actor;
    if (ec) {
        on_shutdown_fail(*child_actor, ec);
    }
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto points = sup.address_mapping.destructive_get(*actor);
    if (!points.empty()) {
        auto cb = [this, child_actor = child_actor, count = points.size()]() mutable {
            --count;
            if (count == 0) {
                remove_child(*child_actor);
            }
        };
        auto cb_ptr = std::make_shared<payload::callback_t>(std::move(cb));
        for (auto &point : points) {
            sup.unsubscribe(point.handler, point.address, cb_ptr);
        }
    } else {
        remove_child(*child_actor);
    }
}

bool children_manager_plugin_t::handle_init(message::init_request_t*) noexcept {
    return initializing_actors.empty();
}

bool children_manager_plugin_t::handle_shutdown(message::shutdown_request_t*) noexcept {
    unsubscribe_all();
    return actors_map.size() == 1; /* only own actor left, which will be handled differently */
}

void children_manager_plugin_t::unsubscribe_all() noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    for(auto& it: actors_map) {
        auto& actor_state = it.second;
        if (!actor_state.shutdown_requesting) {
            auto& actor_addr = it.first;
            if ((actor_addr != sup.get_address()) || sup.parent) {
                auto& timeout = actor->shutdown_timeout;
                sup.request<payload::shutdown_request_t>(actor_addr, actor_addr).send(timeout);
            }
            actor_state.shutdown_requesting = true;
        }
    }
}


/*
should trigger

    action_unlink_clients();
    action_unlink_servers();
    action_unsubscribe_self();

*/
