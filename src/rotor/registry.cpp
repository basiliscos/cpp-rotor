//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/registry.h"
#include "rotor/supervisor.h"

using namespace rotor;

void registry_t::configure(plugin::plugin_base_t &plug) noexcept {
    actor_base_t::configure(plug);
    plug.with_casted<plugin::address_maker_plugin_t>([&](auto &p) { p.set_identity("registry", true); });
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
        auto reason = ::make_error(name, ec);
        reply_with_error(request, reason);
        return;
    }

    auto &service_addr = request.payload.request_payload.service_addr;
    registered_map.emplace(name, service_addr);
    auto &names = reverse_map[service_addr];
    names.insert(name);

    reply_to(request);
    auto predicate = [&](auto &msg) { return msg->payload.request_payload.service_name == name; };
    auto find = [&](auto from) { return std::find_if(from, promises.end(), predicate); };
    auto it = find(promises.begin());
    while (it != promises.end()) {
        reply_to(**it, service_addr);
        it = promises.erase(it);
        it = find(it);
    }
}

void registry_t::on_dereg(message::deregistration_notify_t &message) noexcept {
    auto &service_addr = message.payload.service_addr;
    auto it = reverse_map.find(service_addr);
    if (it != reverse_map.end()) {
        for (auto &name : it->second) {
            registered_map.erase(name);
        }
        reverse_map.erase(it);
    }
}

void registry_t::on_dereg_service(message::deregistration_service_t &message) noexcept {
    auto &name = message.payload.service_name;
    auto it = registered_map.find(name);
    if (it != registered_map.end()) {
        auto &service_addr = it->second;
        auto &names = reverse_map.at(service_addr);
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
        auto reason = ::make_error(name, ec);
        reply_with_error(request, reason);
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
        auto &name = (*it)->payload.request_payload.service_name;
        auto ec = make_error_code(error_code_t::cancelled);
        auto reason = ::make_error(name, ec);
        reply_with_error(**it, reason);
        promises.erase(it);
    }
}
