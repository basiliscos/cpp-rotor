//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "rotor/behavior.h"

using namespace rotor;

behaviour_t::~behaviour_t() {}

actor_shutdown_t::actor_shutdown_t(actor_base_t &actor_) : actor{actor_} {}

void actor_shutdown_t::init() noexcept {
    actor.state = state_t::SHUTTING_DOWN;
    unsubscribe_self();
}

void actor_shutdown_t::unsubscribe_self() noexcept {
    actor_ptr_t self{&actor};
    actor.get_supervisor().unsubscribe_actor(self);
    fn = [this]() { confirm_request(); };
}

void actor_shutdown_t::confirm_request() noexcept {
    if (actor.shutdown_request) {
        actor.reply_to(*actor.shutdown_request);
        actor.shutdown_request.reset();
    }
    return commit_state();
}

void actor_shutdown_t::commit_state() noexcept {
    actor.state = state_t::SHUTTED_DOWN;
    return cleanup();
}

void actor_shutdown_t::cleanup() noexcept {}

supervisor_shutdown_t::supervisor_shutdown_t(supervisor_t &supervisor) : actor_shutdown_t{supervisor} {}

void supervisor_shutdown_t::init() noexcept {
    auto &supervisor = static_cast<supervisor_t &>(actor);
    supervisor.state = state_t::SHUTTING_DOWN;
    shutdown_children();
}

void supervisor_shutdown_t::shutdown_children() noexcept {
    auto &supervisor = static_cast<supervisor_t &>(actor);
    auto &actors_map = supervisor.actors_map;
    for (auto pair : actors_map) {
        auto &addr = pair.first;
        supervisor.request<payload::shutdown_request_t>(addr, addr).timeout(supervisor.shutdown_timeout);
    }
    if (actors_map.empty()) {
        unsubscribe_self();
    } else {
        fn = [this]() { unsubscribe_self(); };
    }
}
