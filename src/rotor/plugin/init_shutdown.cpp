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
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::state>() noexcept { return state; }
template <> auto &actor_base_t::access<to::init_request>() noexcept { return init_request; }
template <> auto &actor_base_t::access<to::shutdown_request>() noexcept { return shutdown_request; }

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
    if (actor->access<to::state>() == state_t::INITIALIZING) {
        actor->access<to::init_request>().reset(&msg);
        plugin_base_t::activate(actor);
        actor->init_continue();
    }
}

void init_shutdown_plugin_t::on_shutdown(message::shutdown_request_t &msg) noexcept {
    assert((actor->access<to::state>() != state_t::SHUTTING_DOWN) ||
           (actor->access<to::state>() != state_t::SHUTTED_DOWN));
    // std::cout << "received shutdown_request for " << actor->address.get() << " from " << msg.payload.reply_to.get()
    // << "\n";
    actor->access<to::shutdown_request>().reset(&msg);
    actor->shutdown_start();
    actor->shutdown_continue();
}

bool init_shutdown_plugin_t::handle_init(message::init_request_t *) noexcept {
    return bool(actor->access<to::init_request>());
}

bool init_shutdown_plugin_t::handle_shutdown(message::shutdown_request_t *) noexcept {
    actor->deactivate_plugins();
    return true;
}
