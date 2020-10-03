//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/registry.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct init_request {};
struct init_timeout {};
struct get_plugin {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::init_request>() noexcept { return init_request; }
template <> auto &actor_base_t::access<to::init_timeout>() noexcept { return init_timeout; }
template <> auto actor_base_t::access<to::get_plugin, const void *>(const void *identity) noexcept {
    return get_plugin(identity);
}

const void *registry_plugin_t::class_identity = static_cast<const void *>(typeid(registry_plugin_t).name());

const void *registry_plugin_t::identity() const noexcept { return class_identity; }

void registry_plugin_t::activate(actor_base_t *actor_) noexcept {
    plugin_base_t::activate(actor_);
    subscribe(&registry_plugin_t::on_registration);
    subscribe(&registry_plugin_t::on_discovery);
    subscribe(&registry_plugin_t::on_future);

    reaction_on(reaction_t::INIT);
    reaction_on(reaction_t::SHUTDOWN);
}

void registry_plugin_t::register_name(const std::string &name, const address_ptr_t &address) noexcept {
    assert(register_map.count(name) == 0 && "name is already registering");
    assert(actor->get_supervisor().get_registry_address());

    assert(!(plugin_state & LINKED));
    if (!(plugin_state & LINKING)) {
        link_registry();
    }

    register_map.emplace(name, register_info_t{address, state_t::REGISTERING});
}

registry_plugin_t::discovery_task_t &registry_plugin_t::discover_name(const std::string &name, address_ptr_t &address,
                                                                      bool delayed) noexcept {
    assert(discovery_map.count(name) == 0 && "name is already discovering");

    assert(!(plugin_state & LINKED));
    if (!(plugin_state & LINKING)) {
        link_registry();
    }

    auto r = discovery_map.emplace(name, discovery_task_t(*this, &address, name, delayed));
    return r.first->second;
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

void registry_plugin_t::on_discovery(message::discovery_response_t &message) noexcept { process_discovery(message); }

void registry_plugin_t::on_future(message::discovery_future_t &message) noexcept { process_discovery(message); }

void registry_plugin_t::continue_init(const std::error_code &ec) noexcept {
    auto &init_request = actor->access<to::init_request>();
    assert(init_request);
    if (ec) {
        actor->reply_with_error(*init_request, ec);
    } else {
        actor->init_continue();
    }
}

void registry_plugin_t::link_registry() noexcept {
    plugin_state = plugin_state | LINKING;
    auto plugin = actor->access<to::get_plugin>(link_client_plugin_t::class_identity);
    auto p = static_cast<link_client_plugin_t *>(plugin);
    auto &registry_addr = actor->get_supervisor().get_registry_address();
    assert(registry_addr && "supervisor has registry address");
    /* we know that registry actor has no I/O, so it is safe to work with it in pre-operational state */
    bool operational_only = false;
    p->link(registry_addr, operational_only, [this](auto &ec) { on_link(ec); });
}

void registry_plugin_t::on_link(const std::error_code &ec) noexcept {
    plugin_state = plugin_state | LINKED;
    plugin_state = plugin_state & ~LINKING;
    if (!ec) {
        auto &registry_addr = actor->get_supervisor().get_registry_address();
        auto timeout = actor->access<to::init_timeout>();
        for (auto &it : register_map) {
            actor->request<payload::registration_request_t>(registry_addr, it.first, it.second.address).send(timeout);
        }
        for (auto &it : discovery_map) {
            auto &task = it.second;
            task.requested = true;
            if (!task.delayed) {
                actor->request<payload::discovery_request_t>(registry_addr, task.service_name).send(timeout);
            } else {
                actor->request<payload::discovery_promise_t>(registry_addr, task.service_name).send(timeout);
            }
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

bool registry_plugin_t::handle_shutdown(message::shutdown_request_t *req) noexcept {
    if (!register_map.empty()) {
        auto &registry_addr = actor->get_supervisor().get_registry_address();
        for (auto &it : register_map) {
            if (it.second.state == state_t::OPERATIONAL) {
                actor->send<payload::deregistration_service_t>(registry_addr, it.first);
                it.second.state = state_t::UNREGISTERING;
            }
        }
        register_map.clear();
    }

    if (!discovery_map.empty()) {
        auto &registry_addr = actor->get_supervisor().get_registry_address();
        for (auto it = discovery_map.begin(); it != discovery_map.end(); ++it) {
            auto &task = it->second;
            if (task.delayed && task.requested) {
                actor->send<payload::discovery_cancel_t>(registry_addr, actor->get_address(), task.service_name);
            }
        }
        discovery_map.clear();
    }
    return plugin_base_t::handle_shutdown(req);
}

bool registry_plugin_t::has_registering() noexcept {
    auto in_progress_predicate = [](auto it) { return it.second.state == state_t::REGISTERING; };
    return !std::none_of(register_map.begin(), register_map.end(), in_progress_predicate);
}

void registry_plugin_t::discovery_task_t::on_discovery(const std::error_code &ec) noexcept {
    if (task_callback)
        task_callback(phase_t::discovering, ec);
    if (!ec) {
        auto p = plugin.actor->access<to::get_plugin>(link_client_plugin_t::class_identity);
        auto &link_plugin = *static_cast<link_client_plugin_t *>(p);
        link_plugin.link(*address, operational_only, [this](auto &ec) {
            if (task_callback)
                task_callback(phase_t::linking, ec);
            continue_init(ec);
        });
        return;
    }
    continue_init(ec);
}

void registry_plugin_t::discovery_task_t::continue_init(const std::error_code &ec) noexcept {
    auto &plugin = this->plugin;
    auto &dm = plugin.discovery_map;
    auto it = dm.find(service_name);
    assert(it != dm.end());
    dm.erase(it);
    if (dm.empty() || ec) {
        plugin.continue_init(ec);
    }
}
