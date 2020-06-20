//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/child_manager.h"
#include "rotor/supervisor.h"
//#include <iostream>

using namespace rotor;
using namespace rotor::internal;

const void* child_manager_plugin_t::class_identity = static_cast<const void *>(typeid(child_manager_plugin_t).name());

const void* child_manager_plugin_t::identity() const noexcept {
    return class_identity;
}

void child_manager_plugin_t::activate(actor_base_t* actor_) noexcept {
    plugin_t::activate(actor_);
    //actor = actor_;
    static_cast<supervisor_t&>(*actor_).manager = this;
    subscribe(&child_manager_plugin_t::on_create);
    subscribe(&child_manager_plugin_t::on_init);
    subscribe(&child_manager_plugin_t::on_shutdown_trigger);
    subscribe(&child_manager_plugin_t::on_shutdown_confirm);
    subscribe(&child_manager_plugin_t::on_state_request);
    actor->install_plugin(*this, slot_t::INIT);
    actor->install_plugin(*this, slot_t::SHUTDOWN);
    actors_map.emplace(actor->get_address(), actor_state_t{actor, false});
    actor->configure(*this);
}


void child_manager_plugin_t::deactivate() noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    assert(actors_map.size() == 1);
    //actors_map.clear();
    initializing_actors.clear();
    if (sup.address_mapping.empty()) plugin_t::deactivate();
}

void child_manager_plugin_t::remove_child(const actor_base_t &child, bool normal_flow) noexcept {
    auto it_actor = actors_map.find(child.address);
    assert(it_actor != actors_map.end());
    actors_map.erase(it_actor);
    if (normal_flow && actors_map.size() == 1) {
        auto state = actor->get_state();
        if (state == state_t::SHUTTING_DOWN) {
            actor->shutdown_continue();
        } else if (state == state_t::INITIALIZING) {
            actor->init_continue();
        }
    }
}

void child_manager_plugin_t::create_child(const actor_ptr_t& child) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    child->do_initialize(sup.get_context());
    auto &timeout = child->get_init_timeout();
    sup.send<payload::create_actor_t>(sup.get_address(), child, timeout);
    if (sup.state == state_t::INITIALIZING) {
        initializing_actors.emplace(child->get_address());
        postponed_init = true;
    }
}

void child_manager_plugin_t::on_create(message::create_actor_t &message) noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto actor = message.payload.actor;
    auto actor_address = actor->get_address();
    actors_map.emplace(actor_address, actor_state_t{std::move(actor), false});
    sup.template request<payload::initialize_actor_t>(actor_address, actor_address).send(message.payload.timeout);
}

void child_manager_plugin_t::on_init(message::init_response_t &message) noexcept {
    auto &address = message.payload.req->payload.request_payload.actor_address;
    auto &ec = message.payload.ec;

    auto &sup = static_cast<supervisor_t &>(*actor);
    bool continue_init = false;
    auto it = initializing_actors.find(address);
    if (it != initializing_actors.end()) {
        initializing_actors.erase(it);
    }
    continue_init = initializing_actors.empty() && !ec && address != actor->address;
    if (ec) {
        auto shutdown_self = actor->state == state_t::INITIALIZING && sup.policy == supervisor_policy_t::shutdown_self;
        if (shutdown_self) {
            continue_init = false;
            sup.do_shutdown();
        } else {
            sup.template request<payload::shutdown_request_t>(address, address).send(sup.shutdown_timeout);
        }
    } else {
        sup.template send<payload::start_actor_t>(address, address);
    }
    if (continue_init && postponed_init) {
        actor->init_continue();
    }
}

void child_manager_plugin_t::on_shutdown_trigger(message::shutdown_trigger_t& message) noexcept {
    if (actors_map.empty()) return; /* already finished / deactivated */
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto &source_addr = message.payload.actor_address;
    if (source_addr == sup.address) {
        if (sup.parent) {
            // will be routed via shutdown request
            sup.do_shutdown();
        } else {
            // do not do shutdown-request on self
            //assert((actor->state != state_t::SHUTTING_DOWN) || (actor->state != state_t::SHUTTED_DOWN));
            if (actor->state != state_t::SHUTTING_DOWN) {
                actor->shutdown_start();
                unsubscribe_all(true);
            }
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

void child_manager_plugin_t::on_shutdown_fail(actor_base_t &actor, const std::error_code &ec) noexcept {
    actor.get_supervisor().get_context()->on_error(ec);
}


void child_manager_plugin_t::on_shutdown_confirm(message::shutdown_response_t& message) noexcept {
    auto &source_addr = message.payload.req->address;
    auto &actor_state = actors_map.at(source_addr);
    actor_state.shutdown_requesting = false;
    auto &ec = message.payload.ec;
    auto child_actor = actor_state.actor;
    if (ec) {
        on_shutdown_fail(*child_actor, ec);
    }
    // std::cout << "shutdown confirmed from " << (void*) source_addr.get() << " on " << (void*)actor->address.get() << "\n";
    auto& sup = static_cast<supervisor_t&>(*actor);
    if (sup.address_mapping.has_subscriptions(*child_actor)) {
        sup.address_mapping.each_subscription(*child_actor, [&](auto &info){
            sup.lifetime->unsubscribe(info);
        });
    } else {
        auto state = sup.get_state();
        bool normal_flow = (state == state_t::OPERATIONAL) || (state == state_t::SHUTTING_DOWN)
            || (state == state_t::INITIALIZING && sup.policy == supervisor_policy_t::shutdown_failed);
        remove_child(*child_actor, normal_flow);
        if (!normal_flow) {
            sup.do_shutdown();
        }
    }
}

void child_manager_plugin_t::on_state_request(message::state_request_t& message) noexcept {
    auto &addr = message.payload.request_payload.subject_addr;
    state_t target_state = state_t::UNKNOWN;
    auto it = actors_map.find(addr);
    if (it != actors_map.end()) {
        auto &state = it->second;
        auto &actor = state.actor;
        target_state = actor->state;
    }
    actor->reply_to(message, target_state);
}


bool child_manager_plugin_t::handle_init(message::init_request_t*) noexcept {
    return initializing_actors.empty();
}

bool child_manager_plugin_t::handle_shutdown(message::shutdown_request_t*) noexcept {
    /* prevent double sending req, i.e. from parent and from self */
    auto& self = actors_map.at(actor->address);
    self.shutdown_requesting = true;
    unsubscribe_all(false);

    return actors_map.size() == 1; /* only own actor left, which will be handled differently */
}

void child_manager_plugin_t::unsubscribe_all(bool continue_shutdown) noexcept {
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
    if (continue_shutdown && actors_map.size() == 1) {
        actor->shutdown_continue();
    }
}


bool child_manager_plugin_t::handle_unsubscription(const subscription_point_t& point, bool external) noexcept {
    if (point.owner_tag == owner_tag_t::SUPERVISOR) {
        auto& sup = static_cast<supervisor_t&>(*actor);
        sup.address_mapping.remove(point);
        if (!sup.address_mapping.has_subscriptions(*point.owner_ptr)) {
            remove_child(*point.owner_ptr, true);
        }
        if (actors_map.size() == 0) {
            plugin_t::deactivate();
        }
        if (sup.address_mapping.empty()) plugin_t::deactivate();
        return false; //handled by lifetime
    }
    else {
        return plugin_t::handle_unsubscription(point, external);
    }
}

/*
should trigger

    action_unlink_clients();
    action_unlink_servers();
    action_unsubscribe_self();

*/
