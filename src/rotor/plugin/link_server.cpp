//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/link_server.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

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
    if (actor->get_state() >= state_t::SHUTTING_DOWN) {
        auto ec = make_error_code(error_code_t::actor_not_linkable);
        actor->reply_with_error(message, ec);
        return;
    }

    auto& client_addr = message.payload.request_payload.client_addr;
    if (linked_clients.find(client_addr) != linked_clients.end()) {
        auto ec = make_error_code(error_code_t::already_linked);
        actor->reply_with_error(message, ec);
        return;
    }

    linked_clients.emplace(client_addr);
    actor->reply_to(message);
}

void link_server_plugin_t::on_unlink_notify(message::unlink_notify_t &message) noexcept {
    auto& client = message.payload.client_addr;
    auto it = linked_clients.find(client);

    // ok, might be some race
    if (it == linked_clients.end()) return;
    linked_clients.erase(it);

    if (actor->state == state_t::SHUTTING_DOWN && actor->shutdown_request)
        actor->shutdown_continue();
}


void link_server_plugin_t::on_unlink_response(message::unlink_response_t &message) noexcept {

}

bool link_server_plugin_t::handle_shutdown(message::shutdown_request_t *) noexcept {
    return linked_clients.empty();
}
