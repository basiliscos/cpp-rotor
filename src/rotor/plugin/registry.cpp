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
    plugin_t::activate(actor_);
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

    assert(!(plugin_state & LINKED));
    if (!(plugin_state & LINKING)) {
        link();
    }

    register_map.emplace(name, register_info_t{address, state_t::REGISTERING});
    return true;
}

bool registry_plugin_t::discover_name(const std::string &name, address_ptr_t &address) noexcept {
    if (discovery_map.count(name))
        return false;
    assert(discovery_map.count(name) == 0);

    assert(!(plugin_state & LINKED));
    if (!(plugin_state & LINKING)) {
        link();
    }

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
        it->second.state = state_t::OPERATIONAL;
    }

    if (!has_registering() || ec) {
        continue_init(ec);
    }
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

    if (discovery_map.empty() || ec) {
        continue_init(ec);
    }
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

void registry_plugin_t::link() noexcept {
    plugin_state = plugin_state | LINKING;
    for (auto plugin : actor->plugins) {
        if (plugin->identity() == link_client_plugin_t::class_identity) {
            auto p = static_cast<link_client_plugin_t *>(plugin);
            auto registry_addr = actor->get_supervisor().get_registry();
            p->link(registry_addr, [this](auto &ec) { on_link(ec); });
        }
    }
}

void registry_plugin_t::on_link(const std::error_code &ec) noexcept {
    plugin_state = plugin_state | LINKED;
    plugin_state = plugin_state & ~LINKING;
    if (!ec) {
        auto registry_addr = actor->get_supervisor().get_registry();
        auto timeout = actor->init_timeout;
        for (auto &it : register_map) {
            actor->request<payload::registration_request_t>(registry_addr, it.first, it.second.address).send(timeout);
        }
        for (auto &it : discovery_map) {
            actor->request<payload::discovery_request_t>(registry_addr, it.first).send(timeout);
        }
    } else {
        // no-op as linked_client plugin should already reply with error to init-request
    }
}

bool registry_plugin_t::handle_init(message::init_request_t *) noexcept {
    if (!(plugin_state & CONFIGURED)) {
        actor->configure(*this);
        plugin_state = plugin_state | CONFIGURED;
    }
    return discovery_map.empty() && !has_registering();
}

bool registry_plugin_t::handle_shutdown(message::shutdown_request_t *) noexcept {
    if (register_map.empty())
        return true;
    auto registry_addr = actor->get_supervisor().get_registry();
    for (auto it : register_map) {
        if (it.second.state == state_t::OPERATIONAL) {
            actor->send<payload::deregistration_service_t>(registry_addr, it.first);
            it.second.state = state_t::UNREGISTERING;
        }
    }
    return true;
}

bool registry_plugin_t::has_registering() noexcept {
    auto in_progress_predicate = [](auto it) { return it.second.state == state_t::REGISTERING; };
    return !std::none_of(register_map.begin(), register_map.end(), in_progress_predicate);
}
