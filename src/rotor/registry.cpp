//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/registry.h"
#include "rotor/supervisor.h"

using namespace rotor;

registry_t::~registry_t() {}

void registry_t::init_start() noexcept {
    subscribe(&registry_t::on_reg);
    subscribe(&registry_t::on_dereg);
    subscribe(&registry_t::on_dereg_service);
    subscribe(&registry_t::on_discovery);
    actor_base_t::init_start();
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
