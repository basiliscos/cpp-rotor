//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/plugin.h"
#include "rotor/actor_base.h"

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
            auto count = std::make_shared<std::atomic_int>(own_subscriptions.size());
            auto cb = [this,  count = std::move(count)]() mutable {
                int left= --(*count);
                if (left == 0) {
                    actor->commit_plugin_deactivation(*this);
                    actor = nullptr;
                    own_subscriptions.clear();
                }
            };
            auto cb_ptr = std::make_shared<payload::callback_t>(std::move(cb));
            auto lifetime = actor->lifetime;
            assert(lifetime);

            auto& subs = own_subscriptions;
            for(auto rit = subs.rbegin(); rit != subs.rend(); ++rit) {
                auto& point = *rit;
                lifetime->unsubscribe(point.handler, point.address, cb_ptr);
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

processing_result_t plugin_t::handle_subscription(message::subscription_t&) noexcept {
    std::abort();
}

processing_result_t plugin_t::handle_unsubscription(message::unsubscription_t&) noexcept {
    std::abort();
}

processing_result_t plugin_t::handle_unsubscription_external(message::unsubscription_external_t&) noexcept {
    std::abort();
}

