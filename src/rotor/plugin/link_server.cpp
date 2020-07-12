//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/link_server.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

namespace {
namespace to {
struct address {};
struct state {};
struct shutdown_request {};
struct shutdown_timeout {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::address>() noexcept { return address; }
template <> auto &actor_base_t::access<to::state>() noexcept { return state; }
template <> auto &actor_base_t::access<to::shutdown_request>() noexcept { return shutdown_request; }
template <> auto &actor_base_t::access<to::shutdown_timeout>() noexcept { return shutdown_timeout; }

const void *link_server_plugin_t::class_identity = static_cast<const void *>(typeid(link_server_plugin_t).name());

const void *link_server_plugin_t::identity() const noexcept { return class_identity; }

void link_server_plugin_t::activate(actor_base_t *actor_) noexcept {
    plugin_t::activate(actor_);
    subscribe(&link_server_plugin_t::on_link_request);
    subscribe(&link_server_plugin_t::on_unlink_response);
    subscribe(&link_server_plugin_t::on_unlink_notify);

    reaction_on(reaction_t::SHUTDOWN);
}

void link_server_plugin_t::on_link_request(message::link_request_t &message) noexcept {
    if (actor->access<to::state>() >= state_t::SHUTTING_DOWN) {
        auto ec = make_error_code(error_code_t::actor_not_linkable);
        actor->reply_with_error(message, ec);
        return;
    }

    auto &client_addr = message.payload.request_payload.client_addr;
    if (linked_clients.find(client_addr) != linked_clients.end()) {
        auto ec = make_error_code(error_code_t::already_linked);
        actor->reply_with_error(message, ec);
        return;
    }

    linked_clients.emplace(client_addr, link_state_t::OPERATIONAL);
    actor->reply_to(message);
}

void link_server_plugin_t::on_unlink_notify(message::unlink_notify_t &message) noexcept {
    auto &client = message.payload.client_addr;
    auto it = linked_clients.find(client);

    // ok, might be some race
    if (it == linked_clients.end())
        return;
    linked_clients.erase(it);

    auto &state = actor->access<to::state>();
    auto &shutdown_request = actor->access<to::shutdown_request>();
    if (state == state_t::SHUTTING_DOWN && shutdown_request)
        actor->shutdown_continue();
}

void link_server_plugin_t::on_unlink_response(message::unlink_response_t &message) noexcept {
    auto &ec = message.payload.ec;
    if (ec) {
        actor->reply_with_error(*actor->access<to::shutdown_request>(), ec);
        return;
    }

    auto &client = message.payload.res.client_addr;
    auto it = linked_clients.find(client);

    // ok, might be some race
    if (it == linked_clients.end())
        return;
    linked_clients.erase(it);

    auto &state = actor->access<to::state>();
    auto &shutdown_request = actor->access<to::shutdown_request>();
    if (state == state_t::SHUTTING_DOWN && shutdown_request)
        actor->shutdown_continue();
}

bool link_server_plugin_t::handle_shutdown(message::shutdown_request_t *) noexcept {
    for (auto it : linked_clients) {
        if (it.second == link_state_t::OPERATIONAL) {
            auto &self = actor->access<to::address>();
            auto &timeout = actor->access<to::shutdown_timeout>();
            actor->request<payload::unlink_request_t>(it.first, self).send(timeout);
        }
    }
    return linked_clients.empty();
}
