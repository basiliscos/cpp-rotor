//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

plugin_t::~plugin_t() {}

void plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    actor->commit_plugin_activation(*this, true);
}

bool plugin_t::is_complete_for(slot_t) noexcept {
    std::abort();
}

bool plugin_t::is_complete_for(slot_t, const subscription_point_t&) noexcept {
    std::abort();
}

void plugin_t::deactivate() noexcept {
    own_subscriptions.clear();
    actor->commit_plugin_deactivation(*this);
    actor = nullptr;
}

using namespace rotor::internal;

/*

init-order:
    actor_lifetime_plugin_t    (creates main actor address)
    subscription_plugin_t      (subscribes)
    init_shutdown_plugin_t     (subscribes to init/shutdown, installs self on the appropriate slots)

shutdown-order: (reversed init-order)
    init_shutdown_plugin_t      (starts init/shutdown, when triggered on appropriate slot)
    subscription_plugin_t       (finalizes unsubscriptions)
    actor_lifetime_plugin_t     (confirms shutdown to supervisor)

*/

/* subscription_plugin_t (0) */

void subscription_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor_->subscription_plugin = this;
    this->actor = actor_;

    actor->install_plugin(*this, slot_t::UNSUBSCRIPTION);
    // order is important
    subscribe(&subscription_plugin_t::on_unsubscription);
    subscribe(&subscription_plugin_t::on_unsubscription_external);
    subscribe(&subscription_plugin_t::on_subscription);

    plugin_t::activate(actor_);
}


subscription_plugin_t::iterator_t subscription_plugin_t::find_subscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept {
    auto it = points.rbegin();
    while (it != points.rend()) {
        if (it->address == addr && *it->handler == *handler) {
            return --it.base();
        } else {
            ++it;
        }
    }
    assert(0 && "no subscription found");
}

bool subscription_plugin_t::is_complete_for(slot_t slot, const subscription_point_t& point) noexcept {
    if (slot == slot_t::UNSUBSCRIPTION && points.empty() && actor->get_state() == state_t::SHUTTING_DOWN) {
        actor->subscription_plugin = nullptr;
        plugin_t::deactivate();
        return true;
    }
    return false;
}

void subscription_plugin_t::deactivate() noexcept  {
    unsubscribe();
}

void subscription_plugin_t::unsubscribe() noexcept {
    auto it = points.rbegin();
    auto& sup = actor->get_supervisor();
    while (it != points.rend()) {
        auto &addr = it->address;
        auto &handler = it->handler;
        sup.unsubscribe_actor(addr, handler);
        ++it;
    }
}

void subscription_plugin_t::on_subscription(message::subscription_t &msg) noexcept {
    auto point = subscription_point_t{msg.payload.handler, msg.payload.target_address};
    points.push_back(point);
    actor->on_subscription(point);
}

void subscription_plugin_t::on_unsubscription(message::unsubscription_t &msg) noexcept {
    auto &addr = msg.payload.target_address;
    auto &handler = msg.payload.handler;
    auto it = find_subscription(addr, handler);
    auto point = *it; /* copy */
    points.erase(it);
    actor->get_supervisor().commit_unsubscription(addr, handler);
    actor->on_unsubscription(point);
}

void subscription_plugin_t::on_unsubscription_external(message::unsubscription_external_t &msg) noexcept {
    auto &addr = msg.payload.target_address;
    auto &handler = msg.payload.handler;
    auto it = find_subscription(addr, handler);
    auto point = *it; /* copy */
    points.erase(it);
    auto sup_addr = addr->supervisor.get_address();
    actor->send<payload::commit_unsubscription_t>(sup_addr, addr, handler);
    actor->on_unsubscription(point);
}

/* after_shutdown_plugin_t (-1) */
void actor_lifetime_plugin_t::deactivate() noexcept {
    auto& req = actor->shutdown_request;
    if (req) {
        actor->reply_to(*req);
        req.reset();
    }

    actor->get_state() = state_t::SHUTTED_DOWN;
    actor->shutdown_finish();
    plugin_t::deactivate();
}

void actor_lifetime_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    if (!actor_->address) {
        actor_->address = create_address();
    }
    plugin_t::activate(actor_);
}

address_ptr_t actor_lifetime_plugin_t::create_address() noexcept {
    return actor->get_supervisor().make_address();
}

/* init_shutdown_plugin_t */
void init_shutdown_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    subscribe(&init_shutdown_plugin_t::on_shutdown);
    subscribe(&init_shutdown_plugin_t::on_init);

    actor->install_plugin(*this, slot_t::INIT);
    actor->install_plugin(*this, slot_t::SHUTDOWN);
    actor->init_shutdown_plugin = this;
    plugin_t::activate(actor);
}

bool init_shutdown_plugin_t::is_complete_for(slot_t slot) noexcept {
    if (slot == slot_t::INIT) return (bool)init_request;
    return true;
}

void init_shutdown_plugin_t::deactivate() noexcept {
    actor->init_shutdown_plugin = nullptr;
    plugin_t::deactivate();
}

void init_shutdown_plugin_t::confirm_init() noexcept {
    assert(init_request);
    actor->reply_to(*init_request);
    init_request.reset();
}

void init_shutdown_plugin_t::confirm_shutdown() noexcept {
    actor->deactivate_plugins();
}

void init_shutdown_plugin_t::on_init(message::init_request_t& msg) noexcept {
    init_request.reset(&msg);
    actor->init_start();
}

void init_shutdown_plugin_t::on_shutdown(message::shutdown_request_t& msg) noexcept {
    actor->shutdown_request.reset(&msg);
    actor->shutdown_start();
}

/* initializer_plugin_t */
void initializer_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;
    actor->install_plugin(*this, slot_t::INIT);
    actor->install_plugin(*this, slot_t::SUBSCRIPTION);
    actor->init_subscribe(*this);
    plugin_t::activate(actor);
}

bool initializer_plugin_t::is_complete_for(slot_t slot) noexcept {
    if (slot == slot_t::INIT) return tracked.empty();
    std::abort();
}

bool initializer_plugin_t::is_complete_for(slot_t slot, const subscription_point_t& point) noexcept {
    if (slot == slot_t::SUBSCRIPTION) {
        tracked.remove(point);
        return tracked.empty();
    }
    std::abort();
}

/* started_plugin_t */
void starter_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;
    subscribe(&starter_plugin_t::on_start);
    plugin_t::activate(actor);
}

void starter_plugin_t::on_start(message::start_trigger_t&) noexcept {
    actor->state = state_t::OPERATIONAL;
    auto& callback = actor->start_callback;
    if (callback) callback(*actor);
}

/* locality_plugin_t */
void locality_plugin_t::activate(actor_base_t* actor_) noexcept {
    auto& sup =static_cast<supervisor_t&>(*actor_);
    auto& address = sup.address;
    auto parent = sup.parent;
    bool use_other = parent && parent->address->same_locality(*address);
    auto locality_leader = use_other ? parent->locality_leader : &sup;
    sup.locality_leader = locality_leader;
    plugin_t::activate(actor_);
}

/* subscription_support_plugin_t */
void subscription_support_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    auto& sup = static_cast<supervisor_t&>(*actor_);

    subscribe(&subscription_support_plugin_t::on_call);
    subscribe(&subscription_support_plugin_t::on_unsubscription);
    subscribe(&subscription_support_plugin_t::on_unsubscription_external);
    sup.subscription_support = this;
    plugin_t::activate(actor_);
}

void subscription_support_plugin_t::deactivate() noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    sup.subscription_support = nullptr;
    plugin_t::deactivate();
}

void subscription_support_plugin_t::on_call(message::handler_call_t &message) noexcept {
    auto &handler = message.payload.handler;
    auto &orig_message = message.payload.orig_message;
    handler->call(orig_message);
}

void subscription_support_plugin_t::on_unsubscription(message::commit_unsubscription_t &message) noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto& payload = message.payload;
    sup.commit_unsubscription(payload.target_address, payload.handler);
}

void subscription_support_plugin_t::on_unsubscription_external(message::external_subscription_t &message) noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto &handler = message.payload.handler;
    auto &addr = message.payload.target_address;
    assert(&addr->supervisor == &sup);
    sup.subscribe_actor(addr, handler);
}

/* children_manager_plugin_t */
void children_manager_plugin_t::activate(actor_base_t* actor_) noexcept {
    actor = actor_;
    static_cast<supervisor_t&>(*actor_).manager = this;
    subscribe(&children_manager_plugin_t::on_create);
    subscribe(&children_manager_plugin_t::on_init);
    subscribe(&children_manager_plugin_t::on_shutdown_trigger);
    subscribe(&children_manager_plugin_t::on_shutdown_confirm);
    actor->install_plugin(*this, slot_t::INIT);
    actor->install_plugin(*this, slot_t::SHUTDOWN);
    actors_map.emplace(actor->get_address(), actor_state_t{actor, false});
}

void children_manager_plugin_t::remove_child(actor_base_t &child) noexcept {
    auto it_actor = actors_map.find(child.address);
    actors_map.erase(it_actor);
    if (actors_map.empty() && actor->get_state() == state_t::SHUTTING_DOWN) {
        deactivate();
    }
}

void children_manager_plugin_t::deactivate() noexcept {
    static_cast<supervisor_t&>(*actor).manager = nullptr;
    plugin_t::deactivate();
}

void children_manager_plugin_t::create_child(const actor_ptr_t& child) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    child->do_initialize(sup.get_context());
    auto &timeout = child->get_init_timeout();
    sup.send<payload::create_actor_t>(sup.get_address(), child, timeout);
    if (!activated) { // sup.state == state_t::INITIALIZING
        initializing_actors.emplace(child->get_address());
        postponed_init = true;
    }
}
bool children_manager_plugin_t::is_complete_for(slot_t slot) noexcept {
    if (slot == slot_t::INIT) return initializing_actors.empty();
    else if (slot == slot_t::SHUTDOWN) return actors_map.empty();
    return true;
}

void children_manager_plugin_t::on_create(message::create_actor_t &message) noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto actor = message.payload.actor;
    auto actor_address = actor->get_address();
    actors_map.emplace(actor_address, actor_state_t{std::move(actor), false});
    sup.template request<payload::initialize_actor_t>(actor_address, actor_address).send(message.payload.timeout);
}

void children_manager_plugin_t::on_init(message::init_response_t &message) noexcept {
    auto &address = message.payload.req->payload.request_payload.actor_address;
    auto &ec = message.payload.ec;

    auto &sup = static_cast<supervisor_t &>(*actor);
    bool continue_init = false;
    auto it = initializing_actors.find(address);
    if (it != initializing_actors.end()) {
        initializing_actors.erase(it);
        // actor->get_state() == state_t::INITIALIZING;
    }
    continue_init = !activated && initializing_actors.empty() && !ec;
    if (ec) {
        auto shutdown_self = !activated && sup.policy == supervisor_policy_t::shutdown_self;
        if (shutdown_self) {
            continue_init = false;
            sup.do_shutdown();
        } else {
            sup.template request<payload::shutdown_request_t>(address, address).send(sup.shutdown_timeout);
        }
    } else {
        sup.template send<payload::start_actor_t>(address, address);
    }
    if (continue_init) {
        activated = true;
        if (postponed_init) {
            actor->init_start();
        }
        plugin_t::activate(actor);
    }
}

void children_manager_plugin_t::on_shutdown_trigger(message::shutdown_trigger_t& message) noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto &source_addr = message.payload.actor_address;
    if (source_addr == sup.address) {
        if (sup.parent) { // will be routed via shutdown request
            sup.do_shutdown();
        } else {
            sup.shutdown_start();
            for(auto it: actors_map) {
                auto& actor_state = it.second;
                if (!actor_state.shutdown_requesting) {
                    actor_state.shutdown_requesting = true;
                    auto& timeout = actor->shutdown_timeout;
                    auto& actor_addr = it.first;
                    sup.request<payload::shutdown_request_t>(actor_addr, actor->address).send(timeout);
                }
            }
        }
    } else {
        auto &actor_state = actors_map.at(source_addr);
        if (!actor_state.shutdown_requesting) {
            actor_state.shutdown_requesting = true;
            auto& timeout = actor->shutdown_timeout;
            sup.request<payload::shutdown_request_t>(source_addr, source_addr).send(timeout);
        }
    }
}

void children_manager_plugin_t::on_shutdown_fail(actor_base_t &actor, const std::error_code &ec) noexcept {
    actor.get_supervisor().get_context()->on_error(ec);
}


void children_manager_plugin_t::on_shutdown_confirm(message::shutdown_response_t& message) noexcept {
    auto &source_addr = message.payload.req->payload.request_payload.actor_address;
    auto &actor_state = actors_map.at(source_addr);
    actor_state.shutdown_requesting = false;
    auto &ec = message.payload.ec;
    auto child_actor = actor_state.actor;
    if (ec) {
        on_shutdown_fail(*child_actor, ec);
    }
    auto& sup = static_cast<supervisor_t&>(*actor);
    auto points = sup.address_mapping.destructive_get(*actor);
    if (!points.empty()) {
        auto cb = [this, child_actor = child_actor, count = points.size()]() mutable {
            --count;
            if (count == 0) {
                remove_child(*child_actor);
            }
        };
        auto cb_ptr = std::make_shared<payload::callback_t>(std::move(cb));
        for (auto &point : points) {
            sup.unsubscribe(point.handler, point.address, cb_ptr);
        }
    } else {
        remove_child(*child_actor);
    }
}

/*
should trigger

    action_unlink_clients();
    action_unlink_servers();
    action_unsubscribe_self();

*/
