//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/actor_base.h"
#include "rotor/supervisor.h"
#include <iostream>
#include <boost/core/demangle.hpp>

using namespace rotor;

actor_base_t::actor_base_t(actor_config_t &cfg)
    : supervisor{cfg.supervisor}, init_timeout{cfg.init_timeout}, shutdown_timeout{cfg.shutdown_timeout},
      unlink_timeout{cfg.unlink_timeout}, unlink_policy{cfg.unlink_policy}, state{state_t::NEW}, plugins{std::move(cfg.plugins)}
      {
    for(auto plugin : plugins) {
        activating_plugins.insert(plugin->identity());
    }
}

actor_base_t::~actor_base_t() {
    assert (deactivating_plugins.empty());
    for(auto plugin: plugins) {
        delete plugin;
    }
}

void actor_base_t::do_initialize(system_context_t *) noexcept {
    activate_plugins();
}

void actor_base_t::do_shutdown() noexcept {
    send<payload::shutdown_trigger_t>(supervisor->get_address(), address);
}

void actor_base_t::install_plugin(plugin_t& plugin, slot_t slot) noexcept {
    actor_config_t::plugins_t* dest = nullptr;
    switch (slot) {
    case slot_t::INIT: dest = &init_plugins; break;
    case slot_t::SHUTDOWN: dest = &shutdown_plugins; break;
    case slot_t::SUBSCRIPTION: dest = &subscription_plugins; break;
    }
    dest->emplace_back(&plugin);
}

void actor_base_t::uninstall_plugin(plugin_t& plugin, slot_t slot) noexcept {
    actor_config_t::plugins_t* dest = nullptr;
    switch (slot) {
    case slot_t::INIT: dest = &init_plugins; break;
    case slot_t::SHUTDOWN: dest = &shutdown_plugins; break;
    case slot_t::SUBSCRIPTION: dest = &subscription_plugins; break;
    }
    auto it = std::find(dest->begin(), dest->end(), &plugin);
    dest->erase(it);
}


void actor_base_t::activate_plugins() noexcept {
    for(auto plugin : plugins) {
        plugin->activate(this);
    }
}

void actor_base_t::commit_plugin_activation(plugin_t& plugin, bool success) noexcept {
    if (success) {
        activating_plugins.erase(plugin.identity());
    } else {
        deactivate_plugins();
    }
}

void actor_base_t::deactivate_plugins() noexcept {
    for(auto it = plugins.rbegin(); it != plugins.rend(); ++it) {
        auto& plugin = *--(it.base());
        if (plugin->actor) { // may be it is already inactive
            deactivating_plugins.insert(plugin->identity());
            plugin->deactivate();
        }
    }
}

void actor_base_t::commit_plugin_deactivation(plugin_t& plugin) noexcept {
    deactivating_plugins.erase(plugin.identity());
}

void actor_base_t::init_start() noexcept {
    state = state_t::INITIALIZING;
}

void actor_base_t::init_finish() noexcept {
    reply_to(*init_request);
    init_request.reset();
    state = state_t::INITIALIZED;
}

void actor_base_t::on_start() noexcept {
    state = state_t::OPERATIONAL;
}

void actor_base_t::shutdown_start() noexcept {
    state = state_t::SHUTTING_DOWN;
}

void actor_base_t::shutdown_finish() noexcept {
    // shutdown_request might be missing for root supervisor
    if (shutdown_request) {
        reply_to(*shutdown_request);
        // std::cout << "confirming shutdown of " << actor->address.get() << " for " << req->address << "\n";
        shutdown_request.reset();
    }

    //maybe delete plugins here?
    if (!deactivating_plugins.empty()) {
        auto p = *deactivating_plugins.begin();
        assert(!p && "a plugin was not deactivated");
    }
    state = state_t::SHUTTED_DOWN;
}

void actor_base_t::init_continue() noexcept {
    assert(state == state_t::INITIALIZING);
    while (!init_plugins.empty()) {
        auto& plugin = init_plugins.front();
        if (plugin->handle_init(init_request.get())) {
            init_plugins.pop_front();
            continue;
        }
        break;
    }
    if (init_plugins.empty()) {
        init_finish();
    }
}

void actor_base_t::configure(plugin_t&) noexcept { }


void actor_base_t::shutdown_continue() noexcept {
    assert(state == state_t::SHUTTING_DOWN);
    while (!shutdown_plugins.empty()) {
        auto& plugin = shutdown_plugins.back();
        if (plugin->handle_shutdown(shutdown_request.get())) {
            shutdown_plugins.pop_back();
            continue;
        }
        break;
    }
    if (shutdown_plugins.empty()) {
        shutdown_finish();
    }
}

void actor_base_t::unsubscribe(const handler_ptr_t &h, const address_ptr_t &addr) noexcept {
    auto it = lifetime->points.find(subscription_point_t{h, addr});
    assert(it != lifetime->points.end());
    lifetime->unsubscribe(*it);
}

void actor_base_t::unsubscribe() noexcept {
    lifetime->unsubscribe();
}

template <typename Fn, typename Message>
static void poll(actor_config_t::plugins_t& plugins, Message& message, Fn&& fn) {
    for(auto rit = plugins.rbegin(); rit != plugins.rend();) {
        auto it = --rit.base();
        auto plugin = *it;
        auto result = fn(plugin, message);
        if (result) break;
        ++rit;
    }
}

void actor_base_t::on_subscription(message::subscription_t& message) noexcept {
    /*
    auto& point = message.payload.point;
    std::cout << "actor " << point.handler->actor_ptr.get() << " subscribed to "
              << boost::core::demangle((const char*)point.handler->message_type)
              << " at " << (void*)point.address.get() << "\n";
    */
    for(auto rit = subscription_plugins.rbegin(); rit != subscription_plugins.rend(); ) {
        auto& plugin = *rit;
        auto result = plugin->handle_subscription(message);
        switch (result) {
        case processing_result_t::IGNORED: ++rit; continue;
        case processing_result_t::CONSUMED: return;
        case processing_result_t::FINISHED:
            auto it =  subscription_plugins.erase(--rit.base());
            rit = std::reverse_iterator(it);
        }
    }
}

void actor_base_t::on_unsubscription(message::unsubscription_t& message) noexcept {
    /*
    auto& point = message.payload.point;
    std::cout << "actor " << point.handler->actor_ptr.get() << " unsubscribed[i] from "
              << boost::core::demangle((const char*)point.handler->message_type)
              << " at " << (void*)point.address.get() << "\n";
    */
    poll(plugins, message, [](auto& plugin, auto& message) {
        return plugin->handle_unsubscription(message.payload.point, false);
    });
}

void actor_base_t::on_unsubscription_external(message::unsubscription_external_t& message) noexcept {
    /*
    auto& point = message.payload.point;
    std::cout << "actor " << point.handler->actor_ptr.get() << " unsubscribed[e] from "
              << boost::core::demangle((const char*)point.handler->message_type)
              << " at " << (void*)point.address.get() << "\n";
    */
    poll(plugins, message, [](auto& plugin, auto& message) {
        return plugin->handle_unsubscription(message.payload.point, true);
    });
}

address_ptr_t actor_base_t::create_address() noexcept {
    return address_maker->create_address();
}

bool actor_base_t::ready_to_shutdown() noexcept {
    /* just lifetime */
    return deactivating_plugins.size() == 1;
}


/*
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
    if (unlink_timeout.has_value()) {
        auto &server_addr = msg.address;
        auto &client_addr = msg.payload.request_payload.client_addr;
        linked_clients.emplace(details::linkage_t{server_addr, client_addr});
        reply_to(msg);
    } else {
        auto ec = make_error_code(error_code_t::actor_not_linkable);
        reply_with_error(msg, ec);
    }
}

void actor_base_t::on_unlink_notify(message::unlink_notify_t &msg) noexcept {
    auto &client_addr = msg.payload.client_addr;
    auto &server_addr = msg.address;
    behavior->on_unlink(server_addr, client_addr);
}

bool actor_base_t::unlink_request(const address_ptr_t &service_addr, const address_ptr_t &client_addr) noexcept {
    return behavior->unlink_request(service_addr, client_addr);
}

void actor_base_t::on_unlink_request(message::unlink_request_t &msg) noexcept {
    auto &server_addr = msg.payload.request_payload.server_addr;
    auto it = linked_servers.find(server_addr);
    if (it != linked_servers.end()) {
        auto &client_addr = msg.address;
        if (unlink_request(server_addr, client_addr)) {
            reply_to(msg, server_addr);
            linked_servers.erase(it);
        }
    }
}

void actor_base_t::on_unlink_response(message::unlink_response_t &msg) noexcept {
    auto &ec = msg.payload.ec;
    auto &server_addr = msg.payload.req->payload.request_payload.server_addr;
    auto &client_addr = msg.payload.req->address;
    if (ec) {
        behavior->on_unlink_error(ec);
    }
    behavior->on_unlink(server_addr, client_addr);
}

*/
