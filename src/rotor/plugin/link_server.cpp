//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/link_server.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct state {};
struct shutdown_request {};
struct shutdown_timeout {};
struct link_server {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::state>() noexcept { return state; }
template <> auto &actor_base_t::access<to::shutdown_request>() noexcept { return shutdown_request; }
template <> auto &actor_base_t::access<to::shutdown_timeout>() noexcept { return shutdown_timeout; }
template <> auto &actor_base_t::access<to::link_server>() noexcept { return link_server; }

const std::type_index link_server_plugin_t::class_identity = typeid(link_server_plugin_t);

const std::type_index &link_server_plugin_t::identity() const noexcept { return class_identity; }

void link_server_plugin_t::activate(actor_base_t *actor_) noexcept {
    plugin_base_t::activate(actor_);
    actor->access<to::link_server>() = this;
    subscribe(&link_server_plugin_t::on_link_request);
    subscribe(&link_server_plugin_t::on_unlink_response);
    subscribe(&link_server_plugin_t::on_unlink_notify);

    reaction_on(reaction_t::SHUTDOWN);
}

void link_server_plugin_t::on_link_request(message::link_request_t &message) noexcept {
    auto state = actor->access<to::state>();
    if (state > state_t::OPERATIONAL) {
        auto ec = make_error_code(error_code_t::actor_not_linkable);
        actor->reply_with_error(message, make_error(ec, {}, &message));
        return;
    }

    auto &client_addr = message.payload.origin;
    if (linked_clients.find(client_addr) != linked_clients.end()) {
        auto ec = make_error_code(error_code_t::already_linked);
        actor->reply_with_error(message, make_error(ec, {}, &message));
        return;
    }

    bool operational_only = message.payload.request_payload.operational_only;
    if (operational_only && state < state_t::OPERATIONAL) {
        linked_clients.emplace(client_addr, link_info_t(link_state_t::PENDING, link_request_ptr_t{&message}));
        reaction_on(reaction_t::START);
    } else {
        linked_clients.emplace(client_addr, link_info_t(link_state_t::OPERATIONAL, link_request_ptr_t{}));
        actor->reply_to(message);
    }
}

void link_server_plugin_t::on_unlink_notify(message::unlink_notify_t &message) noexcept {
    auto &client = message.payload.client_addr;
    auto it = linked_clients.find(client);

    if (it == linked_clients.end())
        return;

    auto &unlink_request = it->second.unlink_request;
    if (unlink_request) {
        actor->get_supervisor().cancel_timer(*unlink_request);
    }
    linked_clients.erase(it);

    auto &state = actor->access<to::state>();
    if (state == state_t::SHUTTING_DOWN)
        actor->shutdown_continue();
}

void link_server_plugin_t::on_unlink_response(message::unlink_response_t &message) noexcept {
    auto &ec = message.payload.ee;
    auto &shutdown_request = actor->access<to::shutdown_request>();
    if (ec && shutdown_request) {
        actor->reply_with_error(*actor->access<to::shutdown_request>(), ec);
        return;
    }

    auto &client = message.payload.res.client_addr;
    auto it = linked_clients.find(client);

    // no idea, how to trigger/cause the race
    assert(it != linked_clients.end());
    /*
    if (it == linked_clients.end())
        return;
    */
    linked_clients.erase(it);

    auto &state = actor->access<to::state>();
    if (state == state_t::SHUTTING_DOWN)
        actor->shutdown_continue();
}

bool link_server_plugin_t::handle_shutdown(message::shutdown_request_t *req) noexcept {
    notify_shutdown();
    return linked_clients.empty() && plugin_base_t::handle_shutdown(req);
}

void link_server_plugin_t::handle_start(message::start_trigger_t *trigger) noexcept {
    for (auto &it : linked_clients) {
        if (it.second.state == link_state_t::PENDING) {
            actor->reply_to(*it.second.request);
            it.second.state = link_state_t::OPERATIONAL;
        }
    }
    return plugin_base_t::handle_start(trigger);
}

void link_server_plugin_t::notify_shutdown() noexcept {
    for (auto &[address, link_info] : linked_clients) {
        if (link_info.state == link_state_t::OPERATIONAL) {
            auto &self = actor->get_address();
            auto &timeout = actor->access<to::shutdown_timeout>();
            auto request_id = actor->request<payload::unlink_request_t>(address, self).send(timeout);
            link_info.state = link_state_t::UNLINKING;
            link_info.unlink_request = request_id;
        }
    }
}
