//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/link_client.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct init_request {};
struct init_timeout {};
struct state {};
struct shutdown_request {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::init_request>() noexcept { return init_request; }
template <> auto &actor_base_t::access<to::init_timeout>() noexcept { return init_timeout; }
template <> auto &actor_base_t::access<to::state>() noexcept { return state; }
template <> auto &actor_base_t::access<to::shutdown_request>() noexcept { return shutdown_request; }

const void *link_client_plugin_t::class_identity = static_cast<const void *>(typeid(link_client_plugin_t).name());

const void *link_client_plugin_t::identity() const noexcept { return class_identity; }

void link_client_plugin_t::activate(actor_base_t *actor_) noexcept {
    plugin_base_t::activate(actor_);
    subscribe(&link_client_plugin_t::on_link_response);
    subscribe(&link_client_plugin_t::on_unlink_request);

    reaction_on(reaction_t::INIT);
    reaction_on(reaction_t::SHUTDOWN);
}

void link_client_plugin_t::link(const address_ptr_t &address, bool operational_only,
                                const link_callback_t &callback) noexcept {
    assert(servers_map.count(address) == 0);
    servers_map.emplace(address, server_record_t{callback, link_state_t::LINKING});
    reaction_on(reaction_t::INIT);
    auto &timeout = actor->access<to::init_timeout>();
    actor->request<payload::link_request_t>(address, operational_only).send(timeout);
}

void link_client_plugin_t::on_link_response(message::link_response_t &message) noexcept {
    auto &address = message.payload.req->address;
    auto &ec = message.payload.ec;
    auto it = servers_map.find(address);
    assert(it != servers_map.end());

    auto &callback = it->second.callback;
    if (callback)
        callback(ec);

    if (ec) {
        servers_map.erase(it);
        auto &init_request = actor->access<to::init_request>();
        if (init_request) {
            actor->reply_with_error(*init_request, ec);
            actor->access<to::init_request>().reset();
        } else {
            actor->do_shutdown();
        }
    } else {
        it->second.state = link_state_t::OPERATIONAL;
        if (actor->access<to::init_request>()) {
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

    if (actor->access<to::state>() == rotor::state_t::SHUTTING_DOWN) {
        if (actor->access<to::shutdown_request>()) {
            actor->shutdown_continue();
        }
    } else {
        actor->do_shutdown();
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
    auto in_progress_predicate = [](auto it) { return it.second.state == link_state_t::LINKING; };
    bool not_linking = std::none_of(servers_map.begin(), servers_map.end(), in_progress_predicate);
    return not_linking;
}

bool link_client_plugin_t::handle_shutdown(message::shutdown_request_t *) noexcept {
    if (servers_map.empty())
        return true;

    auto &source_addr = actor->get_address();
    for (auto it = servers_map.begin(); it != servers_map.end();) {
        if (it->second.state != link_state_t::UNLINKING) {
            actor->send<payload::unlink_notify_t>(it->first, source_addr);
            it = servers_map.erase(it);
        } else {
            ++it;
        }
    }
    return true;
}
