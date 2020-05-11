//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/actor_base.h"
#include "rotor/supervisor.h"
//#include <iostream>
//#include <boost/core/demangle.hpp>

using namespace rotor;

actor_base_t::actor_base_t(actor_config_t &cfg)
    : supervisor{cfg.supervisor}, init_timeout{cfg.init_timeout}, shutdown_timeout{cfg.shutdown_timeout},
      unlink_timeout{cfg.unlink_timeout}, unlink_policy{cfg.unlink_policy}, state{state_t::NEW}, inactive_plugins{std::move(cfg.plugins)},
      start_callback{cfg.start_callback} {}

actor_base_t::~actor_base_t() {
    for(auto plugin: inactive_plugins) {
        delete plugin;
    }
    assert(active_plugins.empty() && "active plugins should not be present during actor destructor");
}

void actor_base_t::do_initialize(system_context_t *) noexcept {
    /*
    if (!address) {
        address = create_address();
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
    supervisor->subscribe_actor(*this, &actor_base_t::on_unlink_request);
    supervisor->subscribe_actor(*this, &actor_base_t::on_unlink_response);
    state = state_t::INITIALIZING;
    */
    state = state_t::INITIALIZING;
    activate_plugins();
}

void actor_base_t::do_shutdown() noexcept { send<payload::shutdown_trigger_t>(supervisor->get_address(), address); }

void actor_base_t::install_plugin(plugin_t& plugin, slot_t slot) noexcept {
    actor_config_t::plugins_t* dest = nullptr;
    switch (slot) {
    case slot_t::INIT: dest = &init_plugins; break;
    case slot_t::SHUTDOWN: dest = &shutdown_plugins; break;
    case slot_t::SUBSCRIPTION: dest = &subscription_plugins; break;
    case slot_t::UNSUBSCRIPTION: dest = &unsubscription_plugins; break;
    default: std::abort();
    }
    if (dest) dest->emplace_back(&plugin);
}

void actor_base_t::activate_plugins() noexcept {
    if (inactive_plugins.size()) {
        auto plugin = inactive_plugins.front();
        plugin->activate(this);
    }
}

void actor_base_t::commit_plugin_activation(plugin_t& plugin, bool success) noexcept {
    if (success) {
        for(auto it = inactive_plugins.begin(); it != inactive_plugins.end(); ) {
            if (*it == &plugin) {
                active_plugins.push_back(*it);
                it = inactive_plugins.erase(it);
                break;
            } else {
                ++it;
            }
        }
        activate_plugins();
    } else {
        deactivate_plugins();
    }
}

void actor_base_t::deactivate_plugins() noexcept {
    if (active_plugins.size()) {
        auto plugin = active_plugins.back();
        plugin->deactivate();
    }
}

void actor_base_t::commit_plugin_deactivation(plugin_t& plugin) noexcept {
    for(auto it = active_plugins.rbegin(); it != active_plugins.rend(); ) {
        if (*it == &plugin) {
            inactive_plugins.push_back(*it);
            active_plugins.erase(it.base());
            break;
        } else {
            ++it;
        }
    }
    deactivate_plugins();
}

/*
void actor_base_t::on_initialize(message::init_request_t &msg) noexcept {
    init_request.reset(&msg);
    init_start();
}

void actor_base_t::on_start(message_t<payload::start_actor_t> &) noexcept {
    if (state == state_t::INITIALIZED) {
        state = state_t::OPERATIONAL;
    }
}

void actor_base_t::on_shutdown(message::shutdown_request_t &msg) noexcept {
    shutdown_request.reset(&msg);
    shutdown_start();
}

void actor_base_t::on_shutdown_trigger(message::shutdown_trigger_t &) noexcept { do_shutdown(); }
*/


void actor_base_t::init_continue() noexcept {
    assert(state == state_t::INITIALIZING);
    while (!init_plugins.empty()) {
        auto& plugin = init_plugins.back();
        if (plugin->handle_init(init_request.get())) {
            init_plugins.pop_back();
            continue;
        }
        break;
    }
    if (init_plugins.empty()) {
        init_finish();
    }

}

void actor_base_t::init_subscribe(internal::initializer_plugin_t&) noexcept { }

void actor_base_t::init_finish() noexcept {
    state = state_t::INITIALIZED;
}

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
}

void actor_base_t::shutdown_finish() noexcept {}

void actor_base_t::unsubscribe(const handler_ptr_t &h, const address_ptr_t &addr,
                               const payload::callback_ptr_t &callback) noexcept {

    auto &dest = h->actor_ptr->address;
    auto point = subscription_point_t{h, addr};
    if (&addr->supervisor == this) {
        send<payload::unsubscription_confirmation_t>(dest, point, callback);
    } else {
        assert(!callback);
        send<payload::external_unsubscription_t>(dest, point);
    }
}

void actor_base_t::unsubscribe() noexcept {
    auto plugin = static_cast<internal::subscription_plugin_t*>(subscription_plugins.front());
    plugin->unsubscribe();
}

template <typename Fn, typename Message>
static void poll(actor_config_t::plugins_t& plugins, Message& message, Fn&& fn) {
    for(auto it = plugins.begin(); it != plugins.end();) {
        auto& plugin = *it;
        auto result = fn(plugin, message);
        switch (result) {
        case processing_result_t::IGNORED: it++; break;
        case processing_result_t::CONSUMED: return;
        case processing_result_t::FINISHED: it = plugins.erase(it); break;
        }
    }
}

void actor_base_t::on_subscription(message::subscription_t& message) noexcept {
    /*
    std::cout << "actor " << point.handler->actor_ptr.get() << " subscribed to "
              << boost::core::demangle((const char*)point.handler->message_type)
              << " at " << (void*)point.address.get() << "\n";
    */
    poll(subscription_plugins, message, [](auto& plugin, auto& message) {
        return plugin->handle_subscription(message);
    });
}

void actor_base_t::on_unsubscription(message::unsubscription_t& message) noexcept {
    /*
    std::cout << "actor " << point.handler->actor_ptr.get() << " unsubscribed[i] from "
              << boost::core::demangle((const char*)point.handler->message_type)
              << " at " << (void*)point.address.get() << "\n";
    */
    poll(unsubscription_plugins, message, [](auto& plugin, auto& message) {
        return plugin->handle_unsubscription(message);
    });
}

void actor_base_t::on_unsubscription_external(message::unsubscription_external_t& message) noexcept {
    /*
    std::cout << "actor " << point.handler->actor_ptr.get() << " unsubscribed[e] from "
              << boost::core::demangle((const char*)point.handler->message_type)
              << " at " << (void*)point.address.get() << "\n";
    */
    poll(unsubscription_plugins, message, [](auto& plugin, auto& message) {
        return plugin->handle_unsubscription_external(message);
    });
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
