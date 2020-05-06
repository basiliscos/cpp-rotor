//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/actor_base.h"
#include "rotor/supervisor.h"
//#include <iostream>

using namespace rotor;

actor_base_t::actor_base_t(actor_config_t &cfg)
    : supervisor{cfg.supervisor}, init_timeout{cfg.init_timeout}, shutdown_timeout{cfg.shutdown_timeout},
      unlink_timeout{cfg.unlink_timeout}, unlink_policy{cfg.unlink_policy}, state{state_t::NEW}, inactive_plugins{std::move(cfg.plugins)},
      init_shutdown_plugin{nullptr}, subscription_plugin{nullptr}  {}

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

#if 0
address_ptr_t actor_base_t::create_address() noexcept { return supervisor->make_address(); }
#endif

void actor_base_t::install_plugin(plugin_t& plugin, slot_t slot) noexcept {
    if (slot == slot_t::INIT) { init_plugins.emplace_back(&plugin); }
    else if (slot == slot_t::SHUTDOWN) { shutdown_plugins.emplace_back(&plugin); }
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

/* behavior->on_start_init(); */
void actor_base_t::init_start() noexcept {
    assert(state == state_t::INITIALIZING);
    bool ok = !init_plugins.empty();
    for(auto plugin: init_plugins) {
        ok = plugin->is_init_ready();
        if (!ok) { break; }
    }
    if (ok) {
        state = state_t::INITIALIZED;
        init_shutdown_plugin->confirm_init();
        init_plugins.clear();
        init_finish();
    }
}

void actor_base_t::init_finish() noexcept {}

void actor_base_t::shutdown_start() noexcept {
    assert(state == state_t::OPERATIONAL);
    state = state_t::SHUTTING_DOWN;
    bool ok = !shutdown_plugins.empty();
    for(auto plugin: shutdown_plugins) {
        ok = plugin->is_shutdown_ready();
        if (!ok) { break; }
    }
    if (ok) {
        init_shutdown_plugin->confirm_shutdown();
        shutdown_plugins.clear();
    }
}

void actor_base_t::shutdown_finish() noexcept {}

/*
void actor_base_t::on_subscription(message_t<payload::subscription_confirmation_t> &msg) noexcept {
    points.push_back(subscription_point_t{msg.payload.handler, msg.payload.target_address});
}
*/

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

void actor_base_t::unsubscribe() noexcept {
    subscription_plugin->unsubscribe();
}


/*
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
*/

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
