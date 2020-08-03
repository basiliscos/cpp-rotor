//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/starter.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct plugins {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::plugins>() noexcept { return plugins; }

const void *starter_plugin_t::class_identity = static_cast<const void *>(typeid(starter_plugin_t).name());

const void *starter_plugin_t::identity() const noexcept { return class_identity; }

void starter_plugin_t::activate(actor_base_t *actor_) noexcept {
    parent_t::activate(actor_);
    reaction_on(reaction_t::SUBSCRIPTION);
    reaction_on(reaction_t::START);
    subscribe(&starter_plugin_t::on_start);
}

plugin_base_t::processing_result_t starter_plugin_t::handle_subscription(message::subscription_t &message) noexcept {
    auto &point = message.payload.point;
    auto it = std::find_if(tracked.begin(), tracked.end(), [&](auto info) { return *info == point; });
    if (it != tracked.end()) {
        tracked.erase(it);
    }
    if (configured && tracked.empty()) {
        actor->init_continue();
        return processing_result_t::FINISHED;
    }
    return processing_result_t::IGNORED;
}

void starter_plugin_t::post_configure() noexcept {
    if (tracked.empty()) {
        reaction_off(reaction_t::INIT);
        reaction_off(reaction_t::SUBSCRIPTION);
    }
}

bool starter_plugin_t::configured_handle_init(message::init_request_t *) noexcept { return tracked.empty(); }

bool starter_plugin_t::handle_start(message::start_trigger_t *) noexcept {
    actor->on_start();
    return true;
}

void starter_plugin_t::on_start(message::start_trigger_t &message) noexcept {
    auto &plugins = actor->access<to::plugins>();
    for (auto rit = plugins.rbegin(); rit != plugins.rend(); ++rit) {
        auto plugin = *rit;
        if (plugin->get_reaction() & plugin_base_t::START) {
            if (plugin->handle_start(&message)) {
                plugin->reaction_off(plugin_base_t::START);
                continue;
            }
            break;
        }
    }
}
