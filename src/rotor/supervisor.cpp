#include "rotor/supervisor.h"

using namespace rotor;

supervisor_t::supervisor_t(supervisor_t *sup) : actor_base_t(sup ? *sup : *this), state{state_t::NEW} {}

void supervisor_t::do_initialize() noexcept {
    state = state_t::INITIALIZED;
    actor_base_t::do_initialize();
    subscribe(&actor_base_t::on_initialize);
    subscribe(&supervisor_t::on_initialize_confirm);
    subscribe(&supervisor_t::on_shutdown);
    subscribe(&supervisor_t::on_shutdown_confirm);
    subscribe(&supervisor_t::on_start);
    subscribe(&supervisor_t::on_create);
    auto addr = supervisor.get_address();
    send<payload::initialize_actor_t>(addr, addr);
}

void supervisor_t::on_create(message_t<payload::create_actor_t> &msg) noexcept {
    auto actor = msg.payload.actor;
    subscribe_actor(*actor, &actor_base_t::on_initialize);
    subscribe_actor(*actor, &actor_base_t::on_start);
    subscribe_actor(*actor, &actor_base_t::on_shutdown);
    auto actor_address = actor->get_address();
    actors_map.emplace(actor_address, std::move(actor));
    send<payload::initialize_actor_t>(address, actor_address);
}

void supervisor_t::on_initialize_confirm(message_t<payload::initialize_confirmation_t> &msg) noexcept {
    send<payload::start_actor_t>(msg.payload.actor_address);
}

void supervisor_t::do_start() noexcept { send<payload::start_actor_t>(address); }

void supervisor_t::do_process() noexcept {
    proccess_subscriptions();
    proccess_unsubscriptions();

    while (outbound.size()) {
        auto &item = outbound.front();
        auto &address = item.address;
        auto message = item.message;
        auto it_subscriptions = subscription_map.find(address);
        bool finished = address->ctx_addr == this && state == state_t::SHUTTED_DOWN;
        bool has_recipients = it_subscriptions != subscription_map.end();
        if (!finished && has_recipients) {
            auto &subscription = it_subscriptions->second;
            auto recipients = subscription.get_recipients(message->get_type_index());
            if (recipients) {
                for (auto it : *recipients) {
                    auto &handler = *it;
                    handler.call(message);
                }
            }
            if (!unsubscription_queue.empty()) {
                proccess_unsubscriptions();
            }
            if (!subscription_queue.empty()) {
                proccess_subscriptions();
            }
        }
        outbound.pop_front();
    }
}

void supervisor_t::proccess_subscriptions() noexcept {
    for (auto it : subscription_queue) {
        auto handler_ptr = it.handler;
        auto address = it.address;
        handler_ptr->raw_actor_ptr->remember_subscription(it);
        subscription_map[address].subscribe(handler_ptr);
    }
    subscription_queue.clear();
}

void supervisor_t::proccess_unsubscriptions() noexcept {
    for (auto it : unsubscription_queue) {
        auto &handler = it.handler;
        auto &address = it.address;
        auto &subscriptions = subscription_map.at(address);
        auto &actor_ptr = handler->raw_actor_ptr;
        auto left = subscriptions.unsubscribe(handler);
        actor_ptr->forget_subscription(it);
        if (!left) {
            subscription_map.erase(address);
        }
    }
    unsubscription_queue.clear();
}

void supervisor_t::unsubscribe_actor(address_ptr_t addr, handler_ptr_t &handler_ptr) noexcept {
    unsubscription_queue.emplace_back(subscription_request_t{handler_ptr, addr});
}

void supervisor_t::unsubscribe_actor(const actor_ptr_t &actor, bool remove_actor) noexcept {
    auto &points = actor->get_subscription_points();
    for (auto it : points) {
        auto &address = it.first;
        auto &handler = it.second;
        unsubscription_queue.emplace_back(subscription_request_t{handler, address});
    }
    if (remove_actor) {
        auto it_actor = actors_map.find(actor->get_address());
        if (it_actor == actors_map.end()) {
            context->on_error(make_error_code(error_code_t::missing_actor));
        }
        actors_map.erase(it_actor);
    }
}

// void supervisor_t::on_start(message_t<payload::start_supervisor_t> &) noexcept {}

void supervisor_t::on_initialize(message_t<payload::initialize_actor_t> &msg) noexcept {
    auto actor_addr = msg.payload.actor_address;
    if (actor_addr == address) {
        state = state_t::OPERATIONAL;
    } else {
        send<payload::initialize_actor_t>(actor_addr, actor_addr);
    }
}

void supervisor_t::on_shutdown(message_t<payload::shutdown_request_t> &msg) noexcept {
    auto &source = msg.payload.actor;
    if (source.get() == this) {
        state = state_t::SHUTTING_DOWN;
        for (auto pair : actors_map) {
            auto addr = pair.first;
            send<payload::shutdown_request_t>(addr);
        }
        if (!actors_map.empty()) {
            start_shutdown_timer();
        } else {
            actor_ptr_t self{this};
            // unsubscribe_actor(self, false);
            send<payload::shutdown_confirmation_t>(supervisor.get_address(), self);
        }
    } else {
        send<payload::shutdown_request_t>(source->get_address(), source);
    }
}

void supervisor_t::on_shutdown_timer_trigger() noexcept {
    auto err = make_error_code(error_code_t::shutdown_timeout);
    context->on_error(err);
}

void supervisor_t::on_shutdown_confirm(message_t<payload::shutdown_confirmation_t> &message) noexcept {
    // std::cout << "supervisor_t::on_shutdown_confirm\n";
    if (actors_map.empty()) {
        // std::cout << "supervisor_t::on_shutdown_confirm (unsubscribing self)\n";
        state = state_t::SHUTTED_DOWN;
        cancel_shutdown_timer();
        actor_ptr_t self{this};
        unsubscribe_actor(self, false);
        send<payload::shutdown_confirmation_t>(supervisor.get_address(), self);
    }
}
