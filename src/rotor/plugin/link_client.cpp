//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/link_client.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct discard_request {};
struct on_unlink {};
struct init_request {};
struct init_timeout {};
struct state {};
struct shutdown_request {};
struct link_server {};
} // namespace to
} // namespace

template <> auto supervisor_t::access<to::discard_request, request_id_t>(request_id_t request_id) noexcept {
    return discard_request(request_id);
}
template <> auto actor_base_t::access<to::on_unlink, const address_ptr_t &>(const address_ptr_t &server_addr) noexcept {
    return on_unlink(server_addr);
}

template <> auto &actor_base_t::access<to::init_request>() noexcept { return init_request; }
template <> auto &actor_base_t::access<to::init_timeout>() noexcept { return init_timeout; }
template <> auto &actor_base_t::access<to::state>() noexcept { return state; }
template <> auto &actor_base_t::access<to::link_server>() noexcept { return link_server; }

const std::type_index link_client_plugin_t::class_identity = typeid(link_client_plugin_t);

const std::type_index &link_client_plugin_t::identity() const noexcept { return class_identity; }

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
    reaction_on(reaction_t::INIT);
    auto &timeout = actor->access<to::init_timeout>();
    auto request_id = actor->request<payload::link_request_t>(address, operational_only).send(timeout);
    servers_map.emplace(address, server_record_t{callback, link_state_t::LINKING, request_id});
}

void link_client_plugin_t::on_link_response(message::link_response_t &message) noexcept {
    auto &address = message.payload.req->address;
    auto &ec = message.payload.ee;
    auto it = servers_map.find(address);
    assert(it != servers_map.end());

    auto callback = it->second.callback;
    if (ec) {
        servers_map.erase(it);
    }

    if (callback) {
        callback(ec);
    }

    if (ec) {
        auto &init_request = actor->access<to::init_request>();
        if (init_request) {
            actor->reply_with_error(*init_request, ec);
            actor->access<to::init_request>().reset();
        } /* else
            if (actor->access<to::state>() == state_t::SHUTTING_DOWN) {
            auto ec_inner = make_error_code(shutdown_code_t::link_failed);
            actor->do_shutdown(make_error(ec_inner, ec));
        } */
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

    unlink_queue.emplace_back(&message);
    try_forget_links(true);
}

void link_client_plugin_t::try_forget_links(bool attempt_shutdown) noexcept {
    if (!actor->access<to::link_server>()->has_clients()) {
        bool unlink_requested = !unlink_queue.empty();
        bool shutdown_needed = false;
        for (const auto &it : unlink_queue) {
            auto &message = *it;
            auto &server_addr = message.payload.request_payload.server_addr;
            auto server_it = servers_map.find(server_addr);
            if (server_it != servers_map.end()) {
                actor->reply_to(message, actor->get_address());
                servers_map.erase(server_it);
                auto result = actor->access<to::on_unlink, const address_ptr_t &>(server_addr);
                shutdown_needed |= result;
            }
        }
        if (attempt_shutdown && shutdown_needed) {
            if (actor->access<to::state>() == rotor::state_t::SHUTTING_DOWN) {
                actor->shutdown_continue();
            } else if (unlink_requested) {
                auto ec = make_error_code(shutdown_code_t::unlink_requested);
                actor->do_shutdown(make_error(ec));
            }
        }
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
        configured = true;
        actor->configure(*this);
    }
    auto in_progress_predicate = [](auto it) { return it.second.state == link_state_t::LINKING; };
    bool not_linking = std::none_of(servers_map.begin(), servers_map.end(), in_progress_predicate);
    return not_linking;
}

bool link_client_plugin_t::handle_shutdown(message::shutdown_request_t *req) noexcept {
    if (servers_map.empty())
        return plugin_base_t::handle_shutdown(req);

    try_forget_links(false);

    auto link_server = actor->access<to::link_server>();
    if (!link_server->has_clients()) {
        auto &source_addr = actor->get_address();
        for (auto it = servers_map.begin(); it != servers_map.end();) {
            actor->send<payload::unlink_notify_t>(it->first, source_addr);
            auto &record = it->second;
            if (record.state == link_state_t::LINKING) {
                actor->get_supervisor().access<to::discard_request>(it->second.request_id);
            }
            it = servers_map.erase(it);
        }
    } else {
        link_server->notify_shutdown();
    }

    if (servers_map.empty()) {
        return plugin_base_t::handle_shutdown(req);
    }

    return false;
}
