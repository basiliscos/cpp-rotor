//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/child_manager.h"
#include "rotor/supervisor.h"
//#include <iostream>

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct state {};
struct init_timeout {};
struct shutdown_timeout {};
struct lifetime {};
struct manager {};
struct address_mapping {};
struct system_context {};
struct policy {};
struct parent {};
struct synchronize_start {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::state>() noexcept { return state; }
template <> auto &actor_base_t::access<to::init_timeout>() noexcept { return init_timeout; }
template <> auto &actor_base_t::access<to::shutdown_timeout>() noexcept { return shutdown_timeout; }
template <> auto &actor_base_t::access<to::lifetime>() noexcept { return lifetime; }
template <> auto &supervisor_t::access<to::manager>() noexcept { return manager; }
template <> auto &supervisor_t::access<to::address_mapping>() noexcept { return address_mapping; }
template <> auto &supervisor_t::access<to::system_context>() noexcept { return context; }
template <> auto &supervisor_t::access<to::policy>() noexcept { return policy; }
template <> auto &supervisor_t::access<to::parent>() noexcept { return parent; }
template <> auto &supervisor_t::access<to::synchronize_start>() noexcept { return synchronize_start; }

const void *child_manager_plugin_t::class_identity = static_cast<const void *>(typeid(child_manager_plugin_t).name());

const void *child_manager_plugin_t::identity() const noexcept { return class_identity; }

void child_manager_plugin_t::activate(actor_base_t *actor_) noexcept {
    plugin_base_t::activate(actor_);
    static_cast<supervisor_t &>(*actor_).access<to::manager>() = this;
    subscribe(&child_manager_plugin_t::on_create);
    subscribe(&child_manager_plugin_t::on_init);
    subscribe(&child_manager_plugin_t::on_shutdown_trigger);
    subscribe(&child_manager_plugin_t::on_shutdown_confirm);
    subscribe(&child_manager_plugin_t::on_state_request);
    reaction_on(reaction_t::INIT);
    reaction_on(reaction_t::SHUTDOWN);
    reaction_on(reaction_t::START);
    actors_map.emplace(actor->get_address(), actor_state_t(actor));
    actor->configure(*this);
}

void child_manager_plugin_t::deactivate() noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    if (sup.access<to::address_mapping>().empty()) {
        if (actors_map.size() == 1)
            remove_child(sup);
        plugin_base_t::deactivate();
    }
}

void child_manager_plugin_t::remove_child(const actor_base_t &child) noexcept {
    auto it_actor = actors_map.find(child.get_address());
    assert(it_actor != actors_map.end());
    bool child_started = it_actor->second.strated;
    actors_map.erase(it_actor);
    auto &state = actor->access<to::state>();

    bool shutdown_self = false;
    bool init_self = false;
    if (state == state_t::INITIALIZING) {
        if (!child_started) {
            auto &policy = static_cast<supervisor_t *>(actor)->access<to::policy>();
            if (policy == supervisor_policy_t::shutdown_failed) {
                actor->do_shutdown();
            } else {
                shutdown_self = true;
            }
        } else {
            init_self = true;
        }
    }
    if (state == state_t::SHUTTING_DOWN) {
        shutdown_self = true;
    }

    if (shutdown_self) {
        if (state != state_t::SHUTTING_DOWN) {
            actor->do_shutdown();
        } else if (actors_map.size() <= 1) {
            actor->shutdown_continue();
        }
    }

    if (init_self && (state == state_t::INITIALIZING)) {
        actor->init_continue();
    }
}

void child_manager_plugin_t::create_child(const actor_ptr_t &child) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    child->do_initialize(sup.access<to::system_context>());
    auto &timeout = child->access<to::init_timeout>();
    sup.send<payload::create_actor_t>(actor->get_address(), child, timeout);
    actors_map.emplace(child->get_address(), actor_state_t(child));
    if (static_cast<actor_base_t &>(sup).access<to::state>() == state_t::INITIALIZING) {
        postponed_init = true;
    }
}

void child_manager_plugin_t::on_create(message::create_actor_t &message) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    auto &actor = message.payload.actor;
    auto &actor_address = actor->get_address();
    assert(actors_map.count(actor_address) == 1);
    sup.template request<payload::initialize_actor_t>(actor_address, actor_address).send(message.payload.timeout);
}

void child_manager_plugin_t::on_init(message::init_response_t &message) noexcept {
    auto &address = message.payload.req->payload.request_payload.actor_address;
    auto &ec = message.payload.ec;

    auto &sup = static_cast<supervisor_t &>(*actor);
    bool continue_init = false;
    auto init_predicate = [&](auto &it) {
        auto &state = it.second.actor->template access<to::state>();
        bool still_initializing =
            (it.first != actor->get_address()) && (state <= state_t::INITIALIZING) && !it.second.initialized;
        return still_initializing;
    };
    bool has_initing = std::any_of(actors_map.begin(), actors_map.end(), init_predicate);
    continue_init = !has_initing && !ec;
    if (ec) {
        auto &self_state = actor->access<to::state>();
        auto &policy = sup.access<to::policy>();
        auto shutdown_self = self_state == state_t::INITIALIZING && policy == supervisor_policy_t::shutdown_self;
        if (shutdown_self) {
            continue_init = false;
            sup.do_shutdown();
        } else {
            auto &timeout = static_cast<actor_base_t &>(sup).access<to::shutdown_timeout>();
            sup.template request<payload::shutdown_request_t>(address, address).send(timeout);
        }
    } else {
        auto it_actor = actors_map.find(address);
        /* the if is needed for the very rare case when supervisor was immediately shutted down
           right after creation */
        if (it_actor != actors_map.end()) {
            it_actor->second.initialized = true;
            if (!sup.access<to::synchronize_start>() || address == actor->get_address()) {
                sup.template send<payload::start_actor_t>(address, address);
                it_actor->second.strated = true;
            }
        }
    }
    if (continue_init && postponed_init && actor->access<to::state>() < state_t::INITIALIZED) {
        actor->init_continue();
    }
}

void child_manager_plugin_t::on_shutdown_trigger(message::shutdown_trigger_t &message) noexcept {
    if (actors_map.empty())
        return; /* already finished / deactivated */
    auto &sup = static_cast<supervisor_t &>(*actor);
    auto &source_addr = message.payload.actor_address;
    if (source_addr == static_cast<actor_base_t &>(sup).get_address()) {
        if (sup.access<to::parent>()) {
            // will be routed via shutdown request
            sup.do_shutdown();
        } else {
            // do not do shutdown-request on self
            // assert((actor->state != state_t::SHUTTING_DOWN) || (actor->state != state_t::SHUTTED_DOWN));
            if (actor->access<to::state>() != state_t::SHUTTING_DOWN) {
                actor->shutdown_start();
                unsubscribe_all(true);
            }
        }
    } else {
        auto &actor_state = actors_map.at(source_addr);
        if (!actor_state.shutdown_requesting) {
            actor_state.shutdown_requesting = true;
            auto &timeout = actor->access<to::shutdown_timeout>();
            sup.request<payload::shutdown_request_t>(source_addr, source_addr).send(timeout);
        }
    }
}

void child_manager_plugin_t::on_shutdown_fail(actor_base_t &actor, const std::error_code &ec) noexcept {
    actor.get_supervisor().access<to::system_context>()->on_error(ec);
}

void child_manager_plugin_t::on_shutdown_confirm(message::shutdown_response_t &message) noexcept {
    auto &source_addr = message.payload.req->address;
    auto &actor_state = actors_map.at(source_addr);
    actor_state.shutdown_requesting = false;
    auto &ec = message.payload.ec;
    auto child_actor = actor_state.actor;
    if (ec) {
        on_shutdown_fail(*child_actor, ec);
    }
    // std::cout << "shutdown confirmed from " << (void*) source_addr.get() << " on " << (void*)actor->address.get() <<
    // "\n";
    auto &sup = static_cast<supervisor_t &>(*actor);
    auto &address_mapping = sup.access<to::address_mapping>();
    if (address_mapping.has_subscriptions(*child_actor)) {
        auto action = [&](auto &info) { static_cast<actor_base_t &>(sup).access<to::lifetime>()->unsubscribe(info); };
        address_mapping.each_subscription(*child_actor, action);
    } else {
        remove_child(*child_actor);
    }
}

void child_manager_plugin_t::on_state_request(message::state_request_t &message) noexcept {
    auto &addr = message.payload.request_payload.subject_addr;
    state_t target_state = state_t::UNKNOWN;
    auto it = actors_map.find(addr);
    if (it != actors_map.end()) {
        auto &state = it->second;
        auto &actor = state.actor;
        target_state = actor->access<to::state>();
    }
    actor->reply_to(message, target_state);
}

bool child_manager_plugin_t::handle_init(message::init_request_t *) noexcept {
    auto init_predicate = [&](auto it) {
        auto &address = actor->get_address();
        auto &state = it.second.actor->template access<to::state>();
        return it.first != address && state <= state_t::INITIALIZING;
    };
    bool has_initing = std::any_of(actors_map.begin(), actors_map.end(), init_predicate);
    return !has_initing;
}

bool child_manager_plugin_t::handle_shutdown(message::shutdown_request_t *) noexcept {
    /* prevent double sending req, i.e. from parent and from self */
    auto &self = actors_map.at(actor->get_address());
    self.shutdown_requesting = true;
    unsubscribe_all(false);

    return actors_map.size() == 1; /* only own actor left, which will be handled differently */
}

void child_manager_plugin_t::unsubscribe_all(bool continue_shutdown) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    for (auto &it : actors_map) {
        auto &actor_state = it.second;
        if (!actor_state.shutdown_requesting) {
            auto &actor_addr = it.first;
            auto &address = static_cast<actor_base_t &>(sup).get_address();
            auto &parent = sup.access<to::parent>();
            if ((actor_addr != address) || parent) {
                auto &timeout = actor->access<to::shutdown_timeout>();
                sup.request<payload::shutdown_request_t>(actor_addr, actor_addr).send(timeout);
            }
            actor_state.shutdown_requesting = true;
        }
    }
    if (continue_shutdown && actors_map.size() == 1) {
        actor->shutdown_continue();
    }
}

bool child_manager_plugin_t::handle_unsubscription(const subscription_point_t &point, bool external) noexcept {
    if (point.owner_tag == owner_tag_t::SUPERVISOR) {
        auto &sup = static_cast<supervisor_t &>(*actor);
        auto &address_mapping = sup.access<to::address_mapping>();
        address_mapping.remove(point);
        if (!address_mapping.has_subscriptions(*point.owner_ptr)) {
            remove_child(*point.owner_ptr);
        }
        if (actors_map.size() == 0) {
            plugin_base_t::deactivate();
        }
        if (address_mapping.empty())
            plugin_base_t::deactivate();
        return false; // handled by lifetime
    } else {
        return plugin_base_t::handle_unsubscription(point, external);
    }
}

bool child_manager_plugin_t::handle_start(message::start_trigger_t *) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    if (sup.access<to::synchronize_start>()) {
        for (auto &it : actors_map) {
            auto &address = it.first;
            if (address == actor->get_address())
                continue;
            sup.template send<payload::start_actor_t>(address, address);
            it.second.strated = true;
        }
    }
    return true;
}
