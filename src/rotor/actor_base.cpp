#include "rotor/actor_base.h"
#include "rotor/supervisor.h"
//#include <iostream>

using namespace rotor;

actor_base_t::actor_base_t(supervisor_t &supervisor_) : state{state_t::NEW}, supervisor{supervisor_} {}

actor_base_t::~actor_base_t() {}

void actor_base_t::do_initialize(system_context_t *) noexcept {
    address = create_address();
    supervisor.subscribe_actor(*this, &actor_base_t::on_initialize);
    supervisor.subscribe_actor(*this, &actor_base_t::on_start);
    supervisor.subscribe_actor(*this, &actor_base_t::on_shutdown);
    state = state_t::INITIALIZING;
}

void actor_base_t::do_shutdown() noexcept {
    actor_ptr_t self{this};
    state = state_t::SHUTTING_DOWN;
    send<payload::shutdown_request_t>(supervisor.get_address(), std::move(self));
}

address_ptr_t actor_base_t::create_address() noexcept { return supervisor.make_address(); }

void actor_base_t::on_initialize(message_t<payload::initialize_actor_t> &) noexcept {
    auto destination = supervisor.get_address();
    state = state_t::INITIALIZED;
    send<payload::initialize_confirmation_t>(destination, address);
}

void actor_base_t::on_start(message_t<payload::start_actor_t> &) noexcept { state = state_t::OPERATIONAL; }

void actor_base_t::on_shutdown(message_t<payload::shutdown_request_t> &) noexcept {
    auto destination = supervisor.get_address();
    state = state_t::SHUTTED_DOWN;
    send<payload::shutdown_confirmation_t>(destination, this);
}

void actor_base_t::remember_subscription(const subscription_request_t &req) noexcept {
    points[req.address].push_back(req.handler);
    // std::cout << "points size: " <<  points.size() << "\n";
}

void actor_base_t::forget_subscription(const subscription_request_t &req) noexcept {
    auto &subs = points[req.address];
    auto it = subs.begin();
    while (it != subs.end()) {
        if (**it == *req.handler) {
            // std::cout << "forgot subscription\n";
            it = subs.erase(it);
        } else {
            ++it;
        }
    }
    if (subs.empty()) {
        points.erase(req.address);
    }
}
