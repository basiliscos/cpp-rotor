//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/link_client.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

const void *link_client_plugin_t::class_identity = static_cast<const void *>(typeid(link_client_plugin_t).name());

const void *link_client_plugin_t::identity() const noexcept { return class_identity; }

void link_client_plugin_t::activate(actor_base_t *actor_) noexcept {
    plugin_t::activate(actor_);
    subscribe(&link_client_plugin_t::on_link_response);
    subscribe(&link_client_plugin_t::on_link_response);

    reaction_on(reaction_t::INIT);
    reaction_on(reaction_t::SHUTDOWN);
}

void link_client_plugin_t::link(address_ptr_t &address) noexcept {
    assert(servers_map.count(address) == 0);
    servers_map.emplace(address, state_t::LINKING);
    auto &source_addr = actor->address;
    actor->request<payload::link_request_t>(address, source_addr).send(actor->init_timeout);
}

void link_client_plugin_t::on_link_response(message::link_response_t &message) noexcept {
    auto &address = message.payload.req->address;
    auto &ec = message.payload.ec;
    auto it = servers_map.find(address);
    assert(it != servers_map.end());

    if (ec) {
        servers_map.erase(it);
        if (actor->init_request) {
            actor->reply_with_error(*actor->init_request, ec);
            actor->init_request.reset();
        } else {
            actor->do_shutdown();
        }
    } else {
        it->second = state_t::OPERATIONAL;
        if (actor->init_request) {
            actor->init_continue();
        }
    }
}

void link_client_plugin_t::forget_link(message::unlink_request_t &message) noexcept {
    auto &server_addr = message.payload.request_payload.server_addr;
    auto it = servers_map.find(server_addr);
    if (it == servers_map.end())
        return;

    actor->reply_to(message, actor->get_address());
    servers_map.erase(it);
    if (actor->get_state() == rotor::state_t::SHUTTING_DOWN && actor->shutdown_request) {
        actor->shutdown_continue();
    }
}

void link_client_plugin_t::on_unlink_request(message::unlink_request_t &message) noexcept {
    /* handled by actor somehow */
    if (unlink_reaction && unlink_reaction(message))
        return;
    forget_link(message);
}

bool link_client_plugin_t::handle_init(message::init_request_t *) noexcept {
    if (!configured) {
        actor->configure(*this);
        configured = true;
    }
    auto in_progress_predicate = [](auto it) { return it.second == state_t::LINKING; };
    bool not_linking = std::none_of(servers_map.begin(), servers_map.end(), in_progress_predicate);
    return not_linking;
}

bool link_client_plugin_t::handle_shutdown(message::shutdown_request_t *) noexcept {
    if (servers_map.empty())
        return true;

    auto &source_addr = actor->address;
    for (auto it = servers_map.begin(); it != servers_map.end();) {
        if (it->second == state_t::OPERATIONAL) {
            actor->send<payload::unlink_notify_t>(it->first, source_addr);
            it = servers_map.erase(it);
        } else {
            ++it;
        }
    }
    return true;
}
