#include "rotor/supervisor.h"
#include <assert.h>

using namespace rotor;

supervisor_t::supervisor_t(supervisor_t *sup) : actor_base_t(*this), state{state_t::NEW}, parent{sup} {}

address_ptr_t supervisor_t::make_address() noexcept { return new address_t{*this}; }

void supervisor_t::do_initialize(system_context_t *ctx) noexcept {
    context = ctx;
    state = state_t::INITIALIZED;
    actor_base_t::do_initialize(ctx);
    subscribe(&supervisor_t::on_call);
    subscribe(&supervisor_t::on_initialize_confirm);
    subscribe(&supervisor_t::on_shutdown_confirm);
    subscribe(&supervisor_t::on_create);
    subscribe(&supervisor_t::on_subscription);
    subscribe(&supervisor_t::on_unsubscription);
    auto addr = supervisor.get_address();
    send<payload::initialize_actor_t>(addr, addr);
}

void supervisor_t::on_create(message_t<payload::create_actor_t> &msg) noexcept {
    auto actor = msg.payload.actor;
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
        auto message = outbound.front();
        auto &dest = message->address;
        outbound.pop_front();
        if (&dest->supervisor == this) { /* subscriptions are handled by me */
            if (state == state_t::SHUTTED_DOWN)
                continue;
            auto it_subscriptions = subscription_map.find(dest);
            if (it_subscriptions != subscription_map.end()) {
                auto &subscription = it_subscriptions->second;
                auto recipients = subscription.get_recipients(message->get_type_index());
                if (recipients) {
                    for (auto &it : *recipients) {
                        if (it.mine) {
                            it.handler->call(message);
                        } else {
                            auto &sup = it.handler->supervisor;
                            auto wrapped_message =
                                make_message<payload::handler_call_t>(sup->get_address(), message, it.handler);
                            sup->enqueue(std::move(wrapped_message));
                        }
                    }
                }
                if (!unsubscription_queue.empty()) {
                    proccess_unsubscriptions();
                }
                if (!subscription_queue.empty()) {
                    proccess_subscriptions();
                }
            }
        } else {
            dest->supervisor.enqueue(std::move(message));
        }
    }
}

void supervisor_t::proccess_subscriptions() noexcept {
    for (auto it : subscription_queue) {
        auto &handler_ptr = it.handler;
        auto &address = it.address;
        handler_ptr->raw_actor_ptr->remember_subscription(it);
        auto subs_info = subscription_map.try_emplace(address, *this);
        subs_info.first->second.subscribe(handler_ptr);
        // assert(&address->supervisor == handler_ptr->supervisor.get());
        // std::cout << "remberign subscription ...\n";
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
        // std::cout << "unsubscribing..., left: " << left << "\n";
    }
    unsubscription_queue.clear();
}

void supervisor_t::unsubscribe_actor(address_ptr_t addr, handler_ptr_t &handler_ptr) noexcept {
    unsubscription_queue.emplace_back(subscription_request_t{handler_ptr, addr});
}

void supervisor_t::unsubscribe_actor(const actor_ptr_t &actor, bool remove_actor) noexcept {
    auto &points = actor->get_subscription_points();
    // std::cout << "unsubscribing actor..." << points.size() << "\n";
    for (auto it : points) {
        auto &addr = it.first;
        if (&addr->supervisor == this) {
            for (auto &handler : it.second) {
                unsubscription_queue.emplace_back(subscription_request_t{handler, addr});
            }
        } else {
            for (auto &handler : it.second) {
                send<payload::external_unsubscription_t>(addr->supervisor.address, addr, handler);
            }
        }
        // std::cout << "unsubscribing point...\n";
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
        actor_ptr_t self{this};
        state = state_t::SHUTTING_DOWN;
        for (auto pair : actors_map) {
            auto &addr = pair.first;
            auto &actor = pair.second;
            send<payload::shutdown_request_t>(addr, actor);
        }
        if (!actors_map.empty()) {
            start_shutdown_timer();
        } else {
            send<payload::shutdown_confirmation_t>(address, self);
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
    if (message.payload.actor.get() != this) {
        unsubscribe_actor(message.payload.actor, true);
    }
    if (actors_map.empty()) {
        // std::cout << "supervisor_t::on_shutdown_confirm (unsubscribing self)\n";
        state = state_t::SHUTTED_DOWN;
        cancel_shutdown_timer();
        actor_ptr_t self{this};
        unsubscribe_actor(self, false);
        if (parent) {
            send<payload::shutdown_confirmation_t>(parent->get_address(), self);
        }
    }
}

void supervisor_t::on_subscription(message_t<payload::external_subscription_t> &message) noexcept {
    auto &handler = message.payload.handler;
    auto &addr = message.payload.addr;
    assert(&addr->supervisor == this);
    subscription_queue.emplace_back(subscription_request_t{handler, addr});
}

void supervisor_t::on_unsubscription(message_t<payload::external_unsubscription_t> &message) noexcept {
    auto &handler = message.payload.handler;
    auto &addr = message.payload.addr;
    assert(&addr->supervisor == this);
    unsubscription_queue.emplace_back(subscription_request_t{handler, addr});
}

void supervisor_t::on_call(message_t<payload::handler_call_t> &message) noexcept {
    auto &handler = message.payload.handler;
    auto &orig_message = message.payload.orig_message;
    handler->call(orig_message);
}
