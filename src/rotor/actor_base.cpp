#include "rotor/actor_base.h"
#include "rotor/supervisor.h"
//#include <iostream>

using namespace rotor;

actor_base_t::actor_base_t(supervisor_t &supervisor_) : supervisor{supervisor_}, state{state_t::NEW} {}

actor_base_t::~actor_base_t() {}

void actor_base_t::do_initialize(system_context_t *) noexcept {
    address = create_address();
    supervisor.subscribe_actor(*this, &actor_base_t::on_unsubscription);
    supervisor.subscribe_actor(*this, &actor_base_t::on_external_unsubscription);
    supervisor.subscribe_actor(*this, &actor_base_t::on_initialize);
    supervisor.subscribe_actor(*this, &actor_base_t::on_start);
    supervisor.subscribe_actor(*this, &actor_base_t::on_shutdown);
    supervisor.subscribe_actor(*this, &actor_base_t::on_subscription);
    state = state_t::INITIALIZING;
}

void actor_base_t::do_shutdown() noexcept { send<payload::shutdown_request_t>(supervisor.get_address(), address); }

address_ptr_t actor_base_t::create_address() noexcept { return supervisor.make_address(); }

void actor_base_t::on_initialize(message_t<payload::initialize_actor_t> &) noexcept {
    auto destination = supervisor.get_address();
    state = state_t::INITIALIZED;
    send<payload::initialize_confirmation_t>(destination, address);
}

void actor_base_t::on_start(message_t<payload::start_actor_t> &) noexcept { state = state_t::OPERATIONAL; }

void actor_base_t::on_shutdown(message_t<payload::shutdown_request_t> &) noexcept {
    state = state_t::SHUTTING_DOWN;
    actor_ptr_t self{this};
    supervisor.unsubscribe_actor(self);
}

void actor_base_t::on_subscription(message_t<payload::subscription_confirmation_t> &msg) noexcept {
    points.push_back(subscription_request_t{msg.payload.handler, msg.payload.addr});
}

void actor_base_t::on_unsubscription(message_t<payload::unsubscription_confirmation_t> &msg) noexcept {
    auto &addr = msg.payload.addr;
    auto &handler = msg.payload.handler;
    remove_subscription(addr, handler);
    supervisor.commit_unsubscription(addr, handler);
    if (points.empty() && state == state_t::SHUTTING_DOWN) {
        state = state_t::SHUTTED_DOWN;
        confirm_shutdown();
    }
}

void actor_base_t::on_external_unsubscription(message_t<payload::external_unsubscription_t> &msg) noexcept {
    auto &addr = msg.payload.addr;
    auto &handler = msg.payload.handler;
    remove_subscription(addr, msg.payload.handler);
    auto &sup_addr = addr->supervisor.address;
    send<payload::commit_unsubscription_t>(sup_addr, addr, handler);
}

void actor_base_t::confirm_shutdown() noexcept {
    auto destination = supervisor.get_address();
    send<payload::shutdown_confirmation_t>(destination, address);
}

void actor_base_t::remove_subscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept {
    auto it = points.rbegin();
    while (it != points.rend()) {
        if (it->address == addr && *it->handler == *handler) {
            auto dit = it.base();
            points.erase(--dit);
            return;
        } else {
            ++it;
        }
    }
    assert(0 && "no subscription found");
}
