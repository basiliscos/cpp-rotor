//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/registry.h"
#include "rotor/supervisor.h"

using namespace rotor;

void registry_t::configure(plugin::plugin_base_t &plug) noexcept {
    actor_base_t::configure(plug);
    plug.with_casted<plugin::starter_plugin_t>(
        [](auto &p) {
            p.subscribe_actor(&registry_t::on_reg);
            p.subscribe_actor(&registry_t::on_dereg);
            p.subscribe_actor(&registry_t::on_dereg_service);
            p.subscribe_actor(&registry_t::on_discovery);
            p.subscribe_actor(&registry_t::on_promise);
            p.subscribe_actor(&registry_t::on_cancel);
        },
        plugin::config_phase_t::PREINIT);
}

void registry_t::on_reg(message::registration_request_t &request) noexcept {
    auto &name = request.payload.request_payload.service_name;
    if (registered_map.find(name) != registered_map.end()) {
        auto ec = make_error_code(error_code_t::already_registered);
        reply_with_error(request, ec);
        return;
    }

    auto &service_addr = request.payload.request_payload.service_addr;
    registered_map.emplace(name, service_addr);
    auto &names = revese_map[service_addr];
    names.insert(name);

    reply_to(request);
    auto predicate = [&](auto &msg) { return msg->payload.request_payload.service_name == name; };
    auto it = std::find_if(promises.begin(), promises.end(), predicate);
    while (it != promises.end()) {
        reply_to(**it, service_addr);
        it = promises.erase(it);
    }
}

void registry_t::on_dereg(message::deregistration_notify_t &message) noexcept {
    auto &service_addr = message.payload.service_addr;
    auto it = revese_map.find(service_addr);
    if (it != revese_map.end()) {
        for (auto &name : it->second) {
            registered_map.erase(name);
        }
        revese_map.erase(it);
    }
}

void registry_t::on_dereg_service(message::deregistration_service_t &message) noexcept {
    auto &name = message.payload.service_name;
    auto it = registered_map.find(name);
    if (it != registered_map.end()) {
        auto &service_addr = it->second;
        auto &names = revese_map.at(service_addr);
        names.erase(name);
        registered_map.erase(it);
    }
}

void registry_t::on_discovery(message::discovery_request_t &request) noexcept {
    auto &name = request.payload.request_payload.service_name;
    auto it = registered_map.find(name);
    if (it != registered_map.end()) {
        auto &service_addr = it->second;
        reply_to(request, service_addr);
    } else {
        auto ec = make_error_code(error_code_t::unknown_service);
        reply_with_error(request, ec);
    }
}

void registry_t::on_promise(message::discovery_promise_t &request) noexcept {
    auto &name = request.payload.request_payload.service_name;
    auto it = registered_map.find(name);
    if (it != registered_map.end()) {
        auto &service_addr = it->second;
        reply_to(request, service_addr);
        return;
    }

    promises.emplace_back(&request);
}

void registry_t::on_cancel(message::discovery_cancel_t &notify) noexcept {
    auto &request_id = notify.payload.id;
    auto &client_addr = notify.payload.source;
    auto predicate = [&](auto &msg) { return msg->payload.id == request_id && msg->payload.origin == client_addr; };
    auto rit = std::find_if(promises.rbegin(), promises.rend(), predicate);
    if (rit != promises.rend()) {
        auto it = --rit.base();
        auto ec = make_error_code(error_code_t::cancelled);
        reply_with_error(**it, ec);
        promises.erase(it);
    }
}
