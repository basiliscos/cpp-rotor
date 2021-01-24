//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/init_shutdown.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct state {};
struct init_request {};
struct shutdown_request {};
struct assign_shutdown_reason {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::state>() noexcept { return state; }
template <> auto &actor_base_t::access<to::init_request>() noexcept { return init_request; }
template <> auto &actor_base_t::access<to::shutdown_request>() noexcept { return shutdown_request; }
template <>
auto actor_base_t::access<to::assign_shutdown_reason, extended_error_ptr_t>(extended_error_ptr_t reason) noexcept {
    return assign_shutdown_reason(std::move(reason));
}

const void *init_shutdown_plugin_t::class_identity = static_cast<const void *>(typeid(init_shutdown_plugin_t).name());

const void *init_shutdown_plugin_t::identity() const noexcept { return class_identity; }

void init_shutdown_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;
    subscribe(&init_shutdown_plugin_t::on_init);
    subscribe(&init_shutdown_plugin_t::on_shutdown);

    reaction_on(reaction_t::INIT);
    reaction_on(reaction_t::SHUTDOWN);
}

void init_shutdown_plugin_t::on_init(message::init_request_t &msg) noexcept {
    /* it could be shutdown, then ignore it */
    actor->access<to::init_request>().reset(&msg);
    if (actor->access<to::state>() == state_t::INITIALIZING) {
        plugin_base_t::activate(actor);
        actor->init_continue();
    }
}

void init_shutdown_plugin_t::on_shutdown(message::shutdown_request_t &msg) noexcept {
    using err_t = extended_error_ptr_t;
    assert((actor->access<to::state>() != state_t::SHUTTING_DOWN) ||
           (actor->access<to::state>() != state_t::SHUT_DOWN));
    // std::cout << "received shutdown_request for " << actor->address.get() << " from " << msg.payload.reply_to.get()
    // << "\n";
    auto &reason = msg.payload.request_payload.reason;
    actor->access<to::shutdown_request>().reset(&msg);
    actor->access<to::assign_shutdown_reason, err_t>(reason);
    actor->shutdown_start();
    actor->shutdown_continue();
}

bool init_shutdown_plugin_t::handle_init(message::init_request_t *) noexcept {
    return bool(actor->access<to::init_request>());
}

bool init_shutdown_plugin_t::handle_shutdown(message::shutdown_request_t *req) noexcept {
    actor->deactivate_plugins();
    return plugin_base_t::handle_shutdown(req);
}
