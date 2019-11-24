//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "rotor/behavior.h"
#include "cassert"

using namespace rotor;

actor_behavior_t::~actor_behavior_t() {}

/* init related */
void actor_behavior_t::on_start_init() noexcept {
    assert(actor.state == state_t::INITIALIZING);
    substate = behavior_state_t::INIT_STARTED;
    if (linking_actors.empty()) {
        action_confirm_init();
    }
}

void actor_behavior_t::action_confirm_init() noexcept {
    if (actor.init_request) {
        actor.reply_to(*actor.init_request);
        actor.init_request.reset();
    }
    actor.state = state_t::INITIALIZED;
    return action_finish_init();
}

void actor_behavior_t::action_finish_init() noexcept {
    actor.init_finish();
    substate = behavior_state_t::INIT_ENDED;
}

/* shutdown related */

void actor_behavior_t::on_start_shutdown() noexcept {
    substate = behavior_state_t::SHUTDOWN_STARTED;
    actor.state = state_t::SHUTTING_DOWN;
    action_unlink_clients();
}

void actor_behavior_t::action_clients_unliked() noexcept {
    substate = behavior_state_t::SHUTDOWN_CLIENTS_FINISHED;
    action_unlink_servers();
    action_unsubscribe_self();
}

void actor_behavior_t::action_unlink_clients() noexcept {
    substate = behavior_state_t::SHUTDOWN_CLIENTS_STARTED;
    auto &linked_clients = actor.linked_clients;
    if (linked_clients.empty()) {
        action_clients_unliked();
    } else {
        for (auto &linkage : actor.linked_clients) {
            auto &server_addr = linkage.server_addr;
            auto &client_addr = linkage.client_addr;
            auto &timeout = actor.unlink_timeout.value();
            actor.template request<payload::unlink_request_t>(client_addr, server_addr).send(timeout);
        }
    }
}

void actor_behavior_t::action_unlink_servers() noexcept {
    auto &linked_servers = actor.linked_servers;
    while (!linked_servers.empty()) {
        actor.unlink_notify(*linked_servers.begin());
    }
}

void actor_behavior_t::action_unsubscribe_self() noexcept {
    substate = behavior_state_t::UNSUBSCRIPTION_STARTED;
    actor_ptr_t self{&actor};
    actor.get_supervisor().unsubscribe_actor(self);
}

void actor_behavior_t::action_confirm_shutdown() noexcept {
    if (actor.shutdown_request) {
        actor.reply_to(*actor.shutdown_request);
        actor.shutdown_request.reset();
    }
    return action_commit_shutdown();
}

void actor_behavior_t::action_commit_shutdown() noexcept {
    assert(actor.state == state_t::SHUTTING_DOWN);
    actor.state = state_t::SHUTTED_DOWN;
    return action_finish_shutdown();
}

void actor_behavior_t::action_finish_shutdown() noexcept {
    actor.shutdown_finish();
    substate = behavior_state_t::SHUTDOWN_ENDED;
}

void actor_behavior_t::on_unsubscription() noexcept {
    assert(substate == behavior_state_t::UNSUBSCRIPTION_STARTED);
    action_confirm_shutdown();
}

/* link-related */
void actor_behavior_t::on_link_response(const address_ptr_t &service_addr, const std::error_code &ec) noexcept {
    auto it = linking_actors.find(service_addr);
    bool remove_it = it != linking_actors.end();
    if (remove_it) {
        linking_actors.erase(service_addr);
    }
    if (ec) {
        if (remove_it) {
            if (actor.init_request) {
                actor.reply_with_error(*actor.init_request, ec);
                actor.init_request.reset();
            }
            actor.do_shutdown();
        }
    } else {
        actor.linked_servers.insert(service_addr);
        if (actor.state == state_t::INITIALIZING && remove_it) {
            on_start_init();
        }
    }
}

void actor_behavior_t::on_link_request(const address_ptr_t &service_addr) noexcept {
    if (actor.state == state_t::INITIALIZING) {
        linking_actors.insert(service_addr);
    }
}

bool actor_behavior_t::unlink_request(const address_ptr_t &service_addr, const address_ptr_t &client_addr) noexcept {
    (void)service_addr;
    (void)client_addr;
    return true;
}

void actor_behavior_t::on_unlink_error(const std::error_code &ec) noexcept {
    if (actor.unlink_policy == unlink_policy_t::escalate) {
        actor.supervisor->get_context()->on_error(ec);
    }
}

void actor_behavior_t::on_unlink(const address_ptr_t &service_addr, const address_ptr_t &client_addr) noexcept {
    auto &linked_clients = actor.linked_clients;
    auto linkage = details::linkage_t{service_addr, client_addr};
    auto it = linked_clients.find(linkage);
    if (it != linked_clients.end()) {
        linked_clients.erase(it);
    }
    if (substate == behavior_state_t::SHUTDOWN_CLIENTS_STARTED) {
        action_clients_unliked();
    }
}

/* supervisor */
supervisor_behavior_t::supervisor_behavior_t(supervisor_t &sup) : actor_behavior_t{sup} {}

void supervisor_behavior_t::on_start_shutdown() noexcept {
    substate = behavior_state_t::SHUTDOWN_STARTED;
    auto &sup = static_cast<supervisor_t &>(actor);
    sup.state = state_t::SHUTTING_DOWN;
    action_shutdown_children();
}

void supervisor_behavior_t::action_shutdown_children() noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    auto &actors_map = sup.actors_map;
    if (!actors_map.empty()) {
        for (auto &pair : actors_map) {
            auto &state = pair.second;
            if (!state.shutdown_requesting) {
                auto &addr = pair.first;
                state.shutdown_requesting = true;
                sup.request<payload::shutdown_request_t>(addr, addr).send(sup.shutdown_timeout);
            }
        }
        substate = behavior_state_t::SHUTDOWN_CHILDREN_STARTED;
    } else {
        return action_unsubscribe_self();
    }
}

void supervisor_behavior_t::on_childen_removed() noexcept {
    assert(substate == behavior_state_t::SHUTDOWN_CHILDREN_STARTED);
    action_unsubscribe_self();
}

void supervisor_behavior_t::on_shutdown_fail(const address_ptr_t &, const std::error_code &ec) noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    sup.context->on_error(ec);
}

void supervisor_behavior_t::on_start_init() noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    (void)sup;
    assert(sup.state == state_t::INITIALIZING);
    substate = behavior_state_t::INIT_STARTED;
    if (initializing_actors.empty()) {
        action_confirm_init();
    }
}

void supervisor_behavior_t::on_create_child(const address_ptr_t &address) noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    if (sup.state == state_t::INITIALIZING) {
        initializing_actors.emplace(address);
    }
}

void supervisor_behavior_t::on_init(const address_ptr_t &address, const std::error_code &ec) noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    bool continue_init = false;
    bool in_init = sup.state == state_t::INITIALIZING;
    auto it = initializing_actors.find(address);
    if (it != initializing_actors.end()) {
        initializing_actors.erase(it);
        continue_init = in_init;
    }
    if (ec) {
        auto shutdown_self = in_init && sup.policy == supervisor_policy_t::shutdown_self;
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
        on_start_init();
    }
}
