//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/plugin.h"
#include "rotor/supervisor.h"

using namespace rotor;

plugin_t::~plugin_t() {}

void plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    actor->commit_plugin_activation(*this, true);
}


void plugin_t::deactivate() noexcept {
    if (actor) {
        if (own_subscriptions.empty()) {
            actor->commit_plugin_deactivation(*this);
            actor = nullptr;
        } else {
            auto lifetime = actor->lifetime;
            assert(lifetime);

            auto& subs = own_subscriptions;
            for(auto rit = subs.rbegin(); rit != subs.rend(); ++rit) {
                lifetime->unsubscribe(*rit);
            }
        }
    }
}

bool plugin_t::handle_shutdown(message::shutdown_request_t*) noexcept {
    return true;
}

bool plugin_t::handle_init(message::init_request_t*) noexcept {
    return true;
}

void plugin_t::forget_subscription(const subscription_info_ptr_t &info) noexcept {
    printf("[-] forgetting %s\n", info->handler->message_type);
    actor->get_supervisor().commit_unsubscription(info);
}

bool plugin_t::forget_subscription(const subscription_point_t& point) noexcept {
    auto& subs = own_subscriptions;
    auto it = subs.find(point);
    if (it != subs.end()) {
        auto& info = *it;
        assert(info->owner_tag == owner_tag_t::PLUGIN);
        forget_subscription(info);
        subs.erase(it);
        if (subs.empty()) {
            actor->commit_plugin_deactivation(*this);
            if (actor->state == state_t::SHUTTING_DOWN) {
                actor->shutdown_continue();
            }
            actor = nullptr;
        }
        return true;
    }
    return false;
}


processing_result_t plugin_t::handle_subscription(message::subscription_t&) noexcept {
    std::abort();
}

bool plugin_t::handle_unsubscription(const subscription_point_t &point, bool) noexcept {
    return forget_subscription(point);
}