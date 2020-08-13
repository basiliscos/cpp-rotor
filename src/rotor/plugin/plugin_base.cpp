//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/plugin_base.h"
#include "rotor/supervisor.h"

using namespace rotor::plugin;
using namespace rotor;

namespace {
namespace to {
struct lifetime {};
struct state {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::lifetime>() noexcept { return lifetime; }
template <> auto &actor_base_t::access<to::state>() noexcept { return state; }

plugin_base_t::~plugin_base_t() {}

void plugin_base_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;
    actor->commit_plugin_activation(*this, true);
    phase = config_phase_t::INITIALIZING;
}

void plugin_base_t::deactivate() noexcept {
    if (actor) {
        if (own_subscriptions.empty()) {
            actor->commit_plugin_deactivation(*this);
            actor = nullptr;
            reaction = 0;
        } else {
            auto lifetime = actor->access<to::lifetime>();
            assert(lifetime);

            auto &subs = own_subscriptions;
            for (auto rit = subs.rbegin(); rit != subs.rend(); ++rit) {
                lifetime->unsubscribe(*rit);
            }
        }
    }
}

bool plugin_base_t::handle_shutdown(message::shutdown_request_t *) noexcept { return true; }

bool plugin_base_t::handle_init(message::init_request_t *) noexcept { return true; }

bool plugin_base_t::handle_start(message::start_trigger_t *) noexcept { return true; }

void plugin_base_t::forget_subscription(const subscription_info_ptr_t &info) noexcept {
    // printf("[-] forgetting %s\n", info->handler->message_type);
    actor->get_supervisor().commit_unsubscription(info);
}

bool plugin_base_t::forget_subscription(const subscription_point_t &point) noexcept {
    auto &subs = own_subscriptions;
    auto it = subs.find(point);
    if (it != subs.end()) {
        auto &info = *it;
        assert(info->owner_tag == owner_tag_t::PLUGIN);
        forget_subscription(info);
        subs.erase(it);
        if (subs.empty()) {
            actor->commit_plugin_deactivation(*this);
            if (actor->access<to::state>() == state_t::SHUTTING_DOWN) {
                actor->shutdown_continue();
            }
            actor = nullptr;
        }
        return true;
    }
    return false;
}

bool plugin_base_t::handle_subscription(message::subscription_t &) noexcept { return false; }

bool plugin_base_t::handle_unsubscription(const subscription_point_t &point, bool external) noexcept {
    if (external) {
        auto act = actor; /* backup */
        if (forget_subscription(point)) {
            auto &sup_addr = static_cast<actor_base_t &>(point.address->supervisor).get_address();
            act->send<payload::commit_unsubscription_t>(sup_addr, point);
            return true;
        }
        return false;
    }
    return forget_subscription(point);
}
