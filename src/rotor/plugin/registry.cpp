//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/registry.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct state {};
struct init_request {};
struct init_timeout {};
struct get_plugin {};
struct on_discovery {};
struct aliases_map {};
struct discovery_map {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::state>() noexcept { return state; }
template <> auto &actor_base_t::access<to::init_request>() noexcept { return init_request; }
template <> auto &actor_base_t::access<to::init_timeout>() noexcept { return init_timeout; }
template <>
auto actor_base_t::access<to::get_plugin, const std::type_index *>(const std::type_index *identity) noexcept {
    return get_plugin(*identity);
}

template <>
auto registry_plugin_t::discovery_task_t::access<to::on_discovery, address_ptr_t *, const extended_error_ptr_t &>(
    address_ptr_t *address, const extended_error_ptr_t &ec) noexcept {
    return on_discovery(address, ec);
}

template <> auto &registry_plugin_t::access<to::aliases_map>() noexcept { return aliases_map; }
template <> auto &registry_plugin_t::access<to::discovery_map>() noexcept { return discovery_map; }

template <typename Message> void process_discovery(registry_plugin_t::discovery_map_t &dm, Message &message) noexcept;

const std::type_index registry_plugin_t::class_identity = typeid(registry_plugin_t);

const std::type_index &registry_plugin_t::identity() const noexcept { return class_identity; }

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
    auto &ee = message.payload.ee;
    auto it = register_map.find(service);
    assert(it != register_map.end());
    if (ee) {
        register_map.erase(it);
    } else {
        it->second.state = state_t::OPERATIONAL;
    }

    if (!has_registering() || ee) {
        continue_init(error_code_t::registration_failed, ee);
    }
}

void registry_plugin_t::on_discovery(message::discovery_response_t &message) noexcept {
    process_discovery(discovery_map, message);
}

void registry_plugin_t::on_future(message::discovery_future_t &message) noexcept {
    process_discovery(discovery_map, message);
}

void registry_plugin_t::continue_init(const error_code_t &possible_ec, const extended_error_ptr_t &root_ec) noexcept {
    auto &init_request = actor->access<to::init_request>();
    assert(init_request);
    if (root_ec) {
        auto reason = make_error(possible_ec, root_ec, init_request);
        actor->reply_with_error(*init_request, reason);
    } else {
        actor->init_continue();
    }
}

void registry_plugin_t::link_registry() noexcept {
    plugin_state = plugin_state | LINKING;
    auto plugin = actor->access<to::get_plugin>(&link_client_plugin_t::class_identity);
    auto p = static_cast<link_client_plugin_t *>(plugin);
    auto &registry_addr = actor->get_supervisor().get_registry_address();
    assert(registry_addr && "supervisor has registry address");
    /* we know that registry actor has no I/O, so it is safe to work with it in pre-operational state */
    bool operational_only = false;
    p->link(registry_addr, operational_only, [this](auto &ec) { on_link(ec); });
}

void registry_plugin_t::on_link(const extended_error_ptr_t &ec) noexcept {
    plugin_state = plugin_state | LINKED;
    plugin_state = plugin_state & ~LINKING;
    if (!ec) {
        auto &registry_addr = actor->get_supervisor().get_registry_address();
        auto timeout = actor->access<to::init_timeout>();
        for (auto &it : register_map) {
            actor->request<payload::registration_request_t>(registry_addr, it.first, it.second.address).send(timeout);
        }
        for (auto &it : discovery_map) {
            it.second.do_discover();
        }
    } else {
        // no-op as linked_client plugin should already reply with error to init-request
    }
}

bool registry_plugin_t::handle_init(message::init_request_t *) noexcept {
    if (!(plugin_state & CONFIGURED)) {
        plugin_state = plugin_state | CONFIGURED;
        actor->configure(*this);
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
        for (auto it = discovery_map.begin(); it != discovery_map.end();) {
            if (it->second.do_cancel()) {
                it = discovery_map.erase(it);
            } else {
                ++it;
            }
        }
        if (!discovery_map.empty())
            return false;
        // discovery_map.clear();
    }

    aliases_map.clear();
    return plugin_base_t::handle_shutdown(req);
}

bool registry_plugin_t::has_registering() noexcept {
    auto in_progress_predicate = [](auto it) { return it.second.state == state_t::REGISTERING; };
    return !std::none_of(register_map.begin(), register_map.end(), in_progress_predicate);
}

template <typename Message> void process_discovery(registry_plugin_t::discovery_map_t &dm, Message &message) noexcept {
    auto &service = message.payload.req->payload.request_payload.service_name;
    auto &ee = message.payload.ee;
    auto it = dm.find(service);
    assert(it != dm.end());
    address_ptr_t *address_value = ee ? nullptr : &message.payload.res.service_addr;
    it->second.template access<to::on_discovery, address_ptr_t *, const extended_error_ptr_t &>(address_value, ee);
}

void registry_plugin_t::discovery_task_t::on_discovery(address_ptr_t *service_addr,
                                                       const extended_error_ptr_t &ec) noexcept {
    if (!ec) {
        *address = *service_addr;
        if (task_callback) {
            task_callback(phase_t::discovering, ec);
        }
    }

    auto actor_state = plugin->actor->access<to::state>();
    if (actor_state == rotor::state_t::INITIALIZING) {
        if (!ec) {
            auto &aliases = plugin->access<to::aliases_map>();
            if (!aliases.count(*address)) {
                auto p = plugin->actor->access<to::get_plugin>(&link_client_plugin_t::class_identity);
                auto &link_plugin = *static_cast<link_client_plugin_t *>(p);
                link_plugin.link(*address, operational_only, [this](auto &ec) {
                    auto &am = plugin->access<to::aliases_map>();
                    auto &dm = plugin->access<to::discovery_map>();
                    for (auto &alias : am.at(*address)) {
                        auto &callback = dm.at(alias).task_callback;
                        if (callback) {
                            callback(phase_t::linking, ec);
                        }
                    }
                    post_discovery(ec);
                });
                aliases.emplace(*address, names_t{service_name});
            } else {
                aliases.at(*address).emplace_back(service_name);
            }
            return;
        }
    }
    post_discovery(ec);
}

void registry_plugin_t::discovery_task_t::post_discovery(const extended_error_ptr_t &ec) noexcept {
    auto plugin = this->plugin;
    auto &dm = plugin->discovery_map;
    auto it = dm.find(service_name);
    assert(it != dm.end());

    auto &am = plugin->aliases_map;
    auto ait = am.find(*it->second.address);
    if (ait != am.end()) {
        for (auto &alias : ait->second) {
            dm.erase(alias);
        }
    } else {
        dm.erase(it);
    }

    if (dm.empty() || ec) {
        auto actor_state = plugin->actor->access<to::state>();
        if (actor_state == rotor::state_t::INITIALIZING) {
            plugin->continue_init(error_code_t::discovery_failed, ec);
        } else {
            plugin->actor->shutdown_continue();
        }
    }
}

void registry_plugin_t::discovery_task_t::do_discover() noexcept {
    state = state_t::DISCOVERING;
    auto &actor = plugin->actor;
    auto &registry_addr = actor->get_supervisor().get_registry_address();
    auto timeout = actor->access<to::init_timeout>();
    if (!delayed) {
        actor->request<payload::discovery_request_t>(registry_addr, service_name).send(timeout);
    } else {
        request_id = actor->request<payload::discovery_promise_t>(registry_addr, service_name).send(timeout);
    }
}

bool registry_plugin_t::discovery_task_t::do_cancel() noexcept {
    if (delayed && state == state_t::DISCOVERING) {
        using payload_t = rotor::message::discovery_cancel_t::payload_t;
        auto &actor = plugin->actor;
        auto &registry_addr = actor->get_supervisor().get_registry_address();
        actor->send<payload_t>(registry_addr, request_id, actor->get_address());
        state = state_t::CANCELLING;
        return false;
    }
    return true;
}
