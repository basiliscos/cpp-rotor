//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/resources.h"
#include "rotor/supervisor.h"
#include <numeric>

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct state {};
struct resources {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::state>() noexcept { return state; }
template <> auto &actor_base_t::access<to::resources>() noexcept { return resources; }

const void *resources_plugin_t::class_identity = static_cast<const void *>(typeid(resources_plugin_t).name());

const void *resources_plugin_t::identity() const noexcept { return class_identity; }

void resources_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;

    reaction_on(reaction_t::INIT);
    reaction_on(reaction_t::SHUTDOWN);

    actor->access<to::resources>() = this;

    return plugin_base_t::activate(actor_);
}

bool resources_plugin_t::handle_init(message::init_request_t *) noexcept {
    if (!configured) {
        actor->configure(*this);
        configured = true;
    }
    return !has_aquired_resources();
}

bool resources_plugin_t::handle_shutdown(message::shutdown_request_t *) noexcept { return !has_aquired_resources(); }

void resources_plugin_t::acquire(resource_id_t id) noexcept {
    if (id >= resources.size()) {
        resources.resize(id + 1);
    }
    ++resources[id];
}

bool resources_plugin_t::release(resource_id_t id) noexcept {
    assert(id < resources.size() && "out of bound resource release");
    auto &value = resources[id];
    assert(value > 0 && "release should be previously acquired before releasing");
    --value;
    auto state = actor->access<to::state>();
    bool has_acquired = has_aquired_resources();
    if (state == state_t::INITIALIZING) {
        if (!has_acquired) {
            actor->init_continue();
        }
    } else if (state == state_t::SHUTTING_DOWN) {
        if (!has_acquired) {
            actor->shutdown_continue();
        }
    }
    return has_acquired;
}

std::uint32_t resources_plugin_t::has(resource_id_t id) noexcept {
    if (id >= resources.size()) return 0;
    return resources[id];
}


bool resources_plugin_t::has_aquired_resources() noexcept {
    auto sum = std::accumulate(resources.begin(), resources.end(), 0);
    return sum > 0;
}
