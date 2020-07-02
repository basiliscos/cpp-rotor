//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/registry.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

const void *registry_plugin_t::class_identity = static_cast<const void *>(typeid(registry_plugin_t).name());

const void *registry_plugin_t::identity() const noexcept { return class_identity; }

void registry_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;
    subscribe(&registry_plugin_t::on_registration);
    subscribe(&registry_plugin_t::on_discovery);

    reaction_on(reaction_t::INIT);
    reaction_on(reaction_t::SHUTDOWN);
}

bool registry_plugin_t::register_name(const std::string &name, const address_ptr_t &address) noexcept {
    if (register_map.count(name))
        return false;
    auto registry_addr = actor->get_supervisor().get_registry();
    assert(registry_addr);
    actor->request<payload::registration_request_t>(registry_addr, name, address).send(actor->init_timeout);
    register_map.emplace(name, state_t::REGISTERING);
    return true;
}

bool registry_plugin_t::discover_name(const std::string &name, address_ptr_t &address) noexcept {
    if (discovery_map.count(name))
        return false;
    assert(discovery_map.count(name) == 0);
    auto registry_addr = actor->get_supervisor().get_registry();
    actor->request<payload::discovery_request_t>(registry_addr, name).send(actor->init_timeout);
    discovery_map.emplace(name, &address);
    return true;
}

void registry_plugin_t::on_registration(message::registration_response_t &message) noexcept {
    auto &service = message.payload.req->payload.request_payload.service_name;
    auto &ec = message.payload.ec;
    auto it = register_map.find(service);
    assert(it != register_map.end());
    if (ec) {
        register_map.erase(it);
    } else {
        it->second = state_t::OPERATIONAL;
    }
    continue_init(ec);
}

void registry_plugin_t::on_discovery(message::discovery_response_t &message) noexcept {
    auto &service = message.payload.req->payload.request_payload.service_name;
    auto &ec = message.payload.ec;
    auto it = discovery_map.find(service);
    assert(it != discovery_map.end());
    if (!ec) {
        *(it->second) = message.payload.res.service_addr;
    }
    discovery_map.erase(it);
    continue_init(ec);
}

void registry_plugin_t::continue_init(const std::error_code &ec) noexcept {
    if (ec) {
        if (actor->init_request) {
            actor->reply_with_error(*actor->init_request, ec);
            actor->init_request.reset();
        } else {
            actor->do_shutdown();
        }
    } else {
        if (actor->init_request) {
            actor->init_continue();
        }
    }
}

bool registry_plugin_t::handle_init(message::init_request_t *) noexcept {
    if (!configured) {
        actor->configure(*this);
        configured = true;
    }
    auto in_progress_predicate = [](auto it) { return it.second == state_t::REGISTERING; };
    bool no_registering = std::none_of(register_map.begin(), register_map.end(), in_progress_predicate);
    return discovery_map.empty() && no_registering;
}

bool registry_plugin_t::handle_shutdown(message::shutdown_request_t *) noexcept {
    if (register_map.empty())
        return true;
    auto registry_addr = actor->get_supervisor().get_registry();
    for (auto it : register_map) {
        if (it.second == state_t::OPERATIONAL) {
            actor->send<payload::deregistration_service_t>(registry_addr, it.first);
            it.second = state_t::UNREGISTERING;
        }
    }
    return true;
}
