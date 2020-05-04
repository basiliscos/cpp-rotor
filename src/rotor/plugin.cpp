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

bool plugin_t::is_init_ready() noexcept {
    std::abort();
}

bool plugin_t::is_shutdown_ready() noexcept {
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

    auto lambda_unsubscribe = rotor::lambda<message::unsubscription_t>([this](auto &msg) {
        auto &addr = msg.payload.target_address;
        auto &handler = msg.payload.handler;
        remove_subscription(addr, handler);
        actor->get_supervisor().commit_unsubscription(addr, handler);
        maybe_deactivate();
    });

    auto lambda_unsubscribe_external = rotor::lambda<message::unsubscription_external_t>([this](auto &msg) {
        auto &addr = msg.payload.target_address;
        auto &handler = msg.payload.handler;
        remove_subscription(addr, handler);
        auto sup_addr = addr->supervisor.get_address();
        actor->send<payload::commit_unsubscription_t>(sup_addr, addr, handler);
        maybe_deactivate();
    });

    auto lambda_subscribe = rotor::lambda<message::subscription_t>([this](auto &msg) {
        points.push_back(subscription_point_t{msg.payload.handler, msg.payload.target_address});
    });

    // order is important
    unsubscription = actor->subscribe(std::move(lambda_unsubscribe));
    extenal_unsubscription = actor->subscribe(std::move(lambda_unsubscribe_external));
    subscription = actor->subscribe(std::move(lambda_subscribe));
    plugin_t::activate(actor_);
}


void subscription_plugin_t::remove_subscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept {
    auto it = points.rbegin();
    while (it != points.rend()) {
        if (it->address == addr && *it->handler == *handler) {
            auto dit = it.base();
            points.erase(--dit);
            return;
        } else {
            ++it;
        }
    }
    assert(0 && "no subscription found");
}

void subscription_plugin_t::maybe_deactivate() noexcept {
    if (points.empty() && actor->get_state() == state_t::SHUTTING_DOWN) {
        actor->subscription_plugin = nullptr;
        actor->commit_plugin_deactivation(*this);
        subscription.reset();
        extenal_unsubscription.reset();
        unsubscription.reset();
    }
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

bool init_shutdown_plugin_t::is_init_ready() noexcept {
    return (bool)init_request;
}

bool init_shutdown_plugin_t::is_shutdown_ready() noexcept {
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


/* started_plugin_t */
void starter_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;
    auto lambda_start = rotor::lambda<message::start_trigger_t>([this](auto &msg) {
        actor->state = state_t::OPERATIONAL;
   });
    start = actor->subscribe(std::move(lambda_start));
    //actor->get_supervisor().subscribe_actor(*actor, &starter_plugin_t::on_start);
    plugin_t::activate(actor);
}

/*
void starter_plugin_t::on_start(message::start_trigger_t&) noexcept {
    actor->state = state_t::OPERATIONAL;
}
*/

void starter_plugin_t::deactivate() noexcept {
    plugin_t::deactivate();
    start.reset();
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
    auto& sup = static_cast<supervisor_t&>(*actor_);
    auto lambda_call = rotor::lambda<message::handler_call_t>([](auto &msg) {
        auto &handler = msg.payload.handler;
        auto &orig_message = msg.payload.orig_message;
        handler->call(orig_message);
    });
    auto lambda_commit_unsubscription = rotor::lambda<message::commit_unsubscription_t>([&sup](auto &msg) {
        sup.commit_unsubscription(msg.payload.target_address, msg.payload.handler);
    });
    auto lambda_external_subscription = rotor::lambda<message::external_subscription_t>([&sup](auto &msg) {
        auto &handler = msg.payload.handler;
        auto &addr = msg.payload.target_address;
        assert(&addr->supervisor == &sup);
        sup.subscribe_actor(addr, handler);
    });

    call = actor_->subscribe(std::move(lambda_call));
    commit_unsubscription = actor_->subscribe(std::move(lambda_commit_unsubscription));
    external_subscription = actor_->subscribe(std::move(lambda_external_subscription));
    sup.subscription_support = this;
    plugin_t::activate(actor_);
}

void subscription_support_plugin_t::deactivate() noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    sup.subscription_support = nullptr;
    plugin_t::deactivate();
    external_subscription.reset();
    commit_unsubscription.reset();
    call.reset();
}

/* children_manager_plugin_t */
void children_manager_plugin_t::activate(actor_base_t* actor_) noexcept {
    auto lambda_create_actor = rotor::lambda<message::create_actor_t>([this](auto &msg) {
        auto& sup = static_cast<supervisor_t&>(*actor);
        auto actor = msg.payload.actor;
        auto actor_address = actor->get_address();
        actors_map.emplace(actor_address, actor_state_t{std::move(actor), false});
        sup.template request<payload::initialize_actor_t>(actor_address, actor_address).send(msg.payload.timeout);
    });

    auto lambda_init_confirm = rotor::lambda<message::init_response_t>([this](auto &msg) {
        auto &addr = msg.payload.req->payload.request_payload.actor_address;
        auto &ec = msg.payload.ec;
        on_init(addr, ec);
    });

    auto lambda_shutdown_trigger = rotor::lambda<message::shutdown_trigger_t>([this](auto &msg) {
        auto& sup = static_cast<supervisor_t&>(*actor);
        auto &source_addr = msg.payload.actor_address;
        if (source_addr == sup.address) {
            if (sup.parent) { // will be routed via shutdown request
                sup.do_shutdown();
            } else {
                sup.shutdown_start();
            }
        } else {
            auto &actor_state = actors_map.at(source_addr);
            if (!actor_state.shutdown_requesting) {
                actor_state.shutdown_requesting = true;
                auto& timeout = actor->shutdown_timeout;
                sup.request<payload::shutdown_request_t>(source_addr, source_addr).send(timeout);
            }
        }
    });

    create_actor = actor_->subscribe(std::move(lambda_create_actor));
    init_confirm = actor_->subscribe(std::move(lambda_init_confirm));
    shutdown_trigger = actor_->subscribe(std::move(lambda_shutdown_trigger));
    this->actor = actor_;
    auto& sup = static_cast<supervisor_t&>(*actor_);
    sup.manager = this;
    actor->install_plugin(*this, slot_t::INIT);
    //plugin_t::activate(actor);
}

void children_manager_plugin_t::remove_child(actor_base_t &child) noexcept {
    auto it_actor = actors_map.find(child.address);
    actors_map.erase(it_actor);
    if (actors_map.empty() && actor->get_state() == state_t::SHUTTING_DOWN) {
        deactivate();
    }
}

void children_manager_plugin_t::deactivate() noexcept {
    auto& sup = static_cast<supervisor_t&>(*actor);
    sup.manager = nullptr;
    plugin_t::deactivate();
    shutdown_trigger.reset();
    init_confirm.reset();
    create_actor.reset();
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

void children_manager_plugin_t::on_init(const address_ptr_t &address, const std::error_code &ec) noexcept {
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

bool children_manager_plugin_t::is_init_ready() noexcept {
    return initializing_actors.empty();
}


/*

should trigger

    action_unlink_clients();
    action_unlink_servers();
    action_unsubscribe_self();

*/
