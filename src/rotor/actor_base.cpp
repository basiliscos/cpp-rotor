//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/actor_base.h"
#include "rotor/supervisor.h"
//#include <iostream>

using namespace rotor;

actor_base_t::actor_base_t(const actor_config_t &cfg)
    : supervisor{cfg.supervisor}, init_timeout{cfg.init_timeout},
      shutdown_timeout{cfg.shutdown_timeout}, state{state_t::NEW}, behavior{nullptr} {}

actor_base_t::~actor_base_t() { delete behavior; }

actor_behavior_t *actor_base_t::create_behavior() noexcept { return new actor_behavior_t(*this); }

void actor_base_t::do_initialize(system_context_t *) noexcept {
    if (!address) {
        address = create_address();
    }
    if (!behavior) {
        behavior = create_behavior();
    }

    supervisor->subscribe_actor(*this, &actor_base_t::on_unsubscription);
    supervisor->subscribe_actor(*this, &actor_base_t::on_external_unsubscription);
    supervisor->subscribe_actor(*this, &actor_base_t::on_initialize);
    supervisor->subscribe_actor(*this, &actor_base_t::on_start);
    supervisor->subscribe_actor(*this, &actor_base_t::on_shutdown);
    supervisor->subscribe_actor(*this, &actor_base_t::on_shutdown_trigger);
    supervisor->subscribe_actor(*this, &actor_base_t::on_subscription);
    supervisor->subscribe_actor(*this, &actor_base_t::on_link_request);
    supervisor->subscribe_actor(*this, &actor_base_t::on_link_response);
    supervisor->subscribe_actor(*this, &actor_base_t::on_unlink_notify);
    state = state_t::INITIALIZING;
}

void actor_base_t::do_shutdown() noexcept { send<payload::shutdown_trigger_t>(supervisor->get_address(), address); }

address_ptr_t actor_base_t::create_address() noexcept { return supervisor->make_address(); }

void actor_base_t::on_initialize(message::init_request_t &msg) noexcept {
    init_request.reset(&msg);
    init_start();
}

void actor_base_t::on_start(message_t<payload::start_actor_t> &) noexcept { state = state_t::OPERATIONAL; }

void actor_base_t::on_shutdown(message::shutdown_request_t &msg) noexcept {
    shutdown_request.reset(&msg);
    shutdown_start();
}

void actor_base_t::on_shutdown_trigger(message::shutdown_trigger_t &) noexcept { do_shutdown(); }

void actor_base_t::init_start() noexcept { behavior->on_start_init(); }

void actor_base_t::init_finish() noexcept {}

void actor_base_t::shutdown_start() noexcept { behavior->on_start_shutdown(); }

void actor_base_t::shutdown_finish() noexcept {}

void actor_base_t::on_subscription(message_t<payload::subscription_confirmation_t> &msg) noexcept {
    points.push_back(subscription_point_t{msg.payload.handler, msg.payload.target_address});
}

void actor_base_t::unsubscribe(const handler_ptr_t &h, const address_ptr_t &addr,
                               const payload::callback_ptr_t &callback) noexcept {

    auto &dest = h->actor_ptr->address;
    if (&addr->supervisor == this) {
        send<payload::unsubscription_confirmation_t>(dest, addr, h, callback);
    } else {
        assert(!callback);
        send<payload::external_unsubscription_t>(dest, addr, h);
    }
}

void actor_base_t::on_unsubscription(message_t<payload::unsubscription_confirmation_t> &msg) noexcept {
    auto &addr = msg.payload.target_address;
    auto &handler = msg.payload.handler;
    remove_subscription(addr, handler);
    supervisor->commit_unsubscription(addr, handler);
    if (points.empty() && state == state_t::SHUTTING_DOWN) {
        behavior->on_unsubscription();
    }
}

void actor_base_t::on_external_unsubscription(message_t<payload::external_unsubscription_t> &msg) noexcept {
    auto &addr = msg.payload.target_address;
    auto &handler = msg.payload.handler;
    remove_subscription(addr, msg.payload.handler);
    auto &sup_addr = addr->supervisor.address;
    send<payload::commit_unsubscription_t>(sup_addr, addr, handler);
    if (points.empty() && state == state_t::SHUTTING_DOWN) {
        behavior->on_unsubscription();
    }
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

void actor_base_t::unlink_notify(const address_ptr_t &service_addr) noexcept {
    send<payload::unlink_notify_t>(service_addr, address);
    linked_servers.erase(service_addr);
}

void actor_base_t::on_link_response(message::link_response_t &msg) noexcept {
    auto &ec = msg.payload.ec;
    auto &service_addr = msg.payload.req->address;
    behavior->on_link_response(service_addr, ec);
}

actor_base_t::timer_id_t actor_base_t::link_request(const address_ptr_t &service_addr,
                                                    const pt::time_duration &timeout) noexcept {
    auto timer_id = request<payload::link_request_t>(service_addr, address).send(timeout);
    behavior->on_link_request(service_addr);
    return timer_id;
}

void actor_base_t::on_link_request(message::link_request_t &msg) noexcept {
    auto &server_addr = msg.address;
    auto &client_addr = msg.payload.request_payload.client_addr;
    linked_clients.emplace(details::linkage_t{server_addr, client_addr});
    reply_to(msg);
}

void actor_base_t::on_unlink_notify(message::unlink_notify_t &msg) noexcept {
    auto &client_addr = msg.payload.client_addr;
    auto &server_addr = msg.address;
    auto linkage = details::linkage_t{server_addr, client_addr};
    auto it = linked_clients.find(linkage);
    if (it != linked_clients.end()) {
        linked_clients.erase(it);
    }
}
