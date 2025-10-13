//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/actor_base.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

template <> auto &plugin_base_t::access<actor_base_t>() noexcept { return actor; }

actor_base_t::actor_base_t(actor_config_t &cfg)
    : spawner_address{cfg.spawner_address}, supervisor{cfg.supervisor}, init_timeout{cfg.init_timeout},
      shutdown_timeout{cfg.shutdown_timeout}, state{state_t::NEW} {
    plugins_storage = cfg.plugins_constructor();
    plugins = plugins_storage->get_plugins();
    if (cfg.escalate_failure) {
        continuation_mask |= ESCALATE_FALIURE;
    }
    if (cfg.autoshutdown_supervisor) {
        continuation_mask |= AUTOSHUTDOWN_SUPERVISOR;
    }
    for (auto plugin : plugins) {
        activating_plugins.insert(&plugin->identity());
    }
}

actor_base_t::~actor_base_t() { assert(deactivating_plugins.empty()); }

void actor_base_t::do_initialize(system_context_t *) noexcept { activate_plugins(); }

void actor_base_t::do_shutdown(const extended_error_ptr_t &reason) noexcept {
    if (state < state_t::SHUTTING_DOWN) {
        extended_error_ptr_t r;
        if (!reason) {
            auto ec = make_error_code(shutdown_code_t::normal);
            r = make_error(ec);
        } else {
            r = reason;
        }
        send<payload::shutdown_trigger_t>(supervisor->address, address, std::move(r));
    }
}

void actor_base_t::redirect(message_ptr_t message, const address_ptr_t &addr, const address_ptr_t &next_addr) noexcept{
    message->address = addr;
    message->next_route = next_addr;
    supervisor->put(std::move(message));
}

void actor_base_t::activate_plugins() noexcept {
    for (auto plugin : plugins) {
        plugin->activate(this);
    }
}

void actor_base_t::commit_plugin_activation(plugin_base_t &plugin, bool success) noexcept {
    if (success) {
        auto begin = std::begin(activating_plugins);
        auto end = std::end(activating_plugins);
        auto &id = plugin.identity();
        auto predicate = [&id](auto it) { return *it == id; };
        auto it = std::find_if(begin, end, predicate);
        assert(it != end);
        activating_plugins.erase(it);
    } else {
        deactivate_plugins();
    }
}

void actor_base_t::deactivate_plugins() noexcept {
    for (auto it = plugins.rbegin(); it != plugins.rend(); ++it) {
        auto &plugin = *--(it.base());
        if (plugin->access<actor_base_t>()) { // may be it is already inactive
            deactivating_plugins.insert(&plugin->identity());
            plugin->deactivate();
        }
    }
}

void actor_base_t::commit_plugin_deactivation(plugin_base_t &plugin) noexcept {
    auto begin = std::begin(deactivating_plugins);
    auto end = std::end(deactivating_plugins);
    auto &id = plugin.identity();
    auto predicate = [&id](auto it) { return *it == id; };
    auto it = std::find_if(begin, end, predicate);
    if (it != end) {
        deactivating_plugins.erase(it);
    }
}

void actor_base_t::init_start() noexcept { state = state_t::INITIALIZING; }

void actor_base_t::init_finish() noexcept {
    reply_to(*init_request);
    init_request.reset();
    state = state_t::INITIALIZED;
}

void actor_base_t::on_start() noexcept { state = state_t::OPERATIONAL; }

void actor_base_t::shutdown_start() noexcept {
    state = state_t::SHUTTING_DOWN;
    if (!spawner_address) {
        if ((continuation_mask & ESCALATE_FALIURE) && shutdown_reason && shutdown_reason->root()->ec) {
            auto &sup = supervisor->get_address();
            auto ee = make_error(make_error_code(error_code_t::failure_escalation), shutdown_reason);
            send<payload::shutdown_trigger_t>(sup, sup, std::move(ee));
        }
        if (continuation_mask & AUTOSHUTDOWN_SUPERVISOR) {
            auto &sup = supervisor->get_address();
            auto ee = make_error(make_error_code(shutdown_code_t::child_down), shutdown_reason);
            send<payload::shutdown_trigger_t>(sup, sup, std::move(ee));
        }
    }
}

void actor_base_t::shutdown_finish() noexcept {
    // shutdown_request might be missing for root supervisor
    if (shutdown_request) {
        reply_to(*shutdown_request);
        // std::cout << "confirming shutdown of " << actor->address.get() << " for " << req->address << "\n";
        shutdown_request.reset();
    }

    // maybe delete plugins here?
    assert(deactivating_plugins.empty() && "plugin was not deactivated");
    while (!timers_map.empty()) {
        cancel_timer(timers_map.begin()->first);
    }
    while (!active_requests.empty()) {
        supervisor->do_cancel_timer(*active_requests.begin());
    }
    /*
    if (!deactivating_plugins.empty()) {
        auto p = *deactivating_plugins.begin();
        (void)p;
        assert(!p && "a plugin was not deactivated");
    }
    */
    state = state_t::SHUT_DOWN;
}

void actor_base_t::init_continue() noexcept {
    assert(state == state_t::INITIALIZING);
    assert(init_request);

    continuation_mask = continuation_mask | PROGRESS_INIT;
    std::size_t in_progress = plugins.size();
    for (auto &plugin : plugins) {
        if (plugin->get_reaction() & plugin_base_t::INIT) {
            if (plugin->handle_init(init_request.get())) {
                plugin->reaction_off(plugin_base_t::INIT);
                --in_progress;
                continue;
            }
            break;
        }
        --in_progress;
    }
    continuation_mask = continuation_mask & ~PROGRESS_INIT;
    if (in_progress == 0) {
        init_finish();
    }
}

void actor_base_t::configure(plugin_base_t &) noexcept {}

void actor_base_t::shutdown_continue() noexcept {
    assert(state == state_t::SHUTTING_DOWN);

    std::size_t in_progress = plugins.size();
    continuation_mask = continuation_mask | PROGRESS_SHUTDOWN;
    for (size_t i = plugins.size(); i > 0; --i) {
        auto plugin = plugins[i - 1];
        if (plugin->get_reaction() & plugin_base_t::SHUTDOWN) {
            if (plugin->handle_shutdown(shutdown_request.get())) {
                plugin->reaction_off(plugin_base_t::SHUTDOWN);
                --in_progress;
                continue;
            }
            break;
        }
        --in_progress;
    }
    continuation_mask = continuation_mask & ~PROGRESS_SHUTDOWN;
    if (in_progress == 0) {
        shutdown_finish();
    }
}

template <typename Fn, typename Message> static void poll(plugins_t &plugins, Message &message, Fn &&fn) {
    for (auto rit = plugins.rbegin(); rit != plugins.rend();) {
        auto it = --rit.base();
        auto plugin = *it;
        auto result = fn(plugin, message);
        if (result)
            break;
        ++rit;
    }
}

void actor_base_t::on_subscription(message::subscription_t &message) noexcept {
    /*
    auto& point = message.payload.point;
    std::cout << "actor " << point.handler->actor_ptr.get() << " subscribed to "
              << boost::core::demangle((const char*)point.handler->message_type)
              << " at " << (void*)point.address.get() << "\n";
    */
    for (size_t i = plugins.size(); i > 0; --i) {
        auto plugin = plugins[i - 1];
        if (plugin->get_reaction() & plugin_base_t::SUBSCRIPTION) {
            auto consumed = plugin->handle_subscription(message);
            if (consumed) {
                plugin->reaction_off(plugin_base_t::SUBSCRIPTION);
            }
        }
    }
}

void actor_base_t::on_unsubscription(message::unsubscription_t &message) noexcept {
    /*
    auto& point = message.payload.point;
    std::cout << "actor " << point.handler->actor_ptr.get() << " unsubscribed[i] from "
              << boost::core::demangle((const char*)point.handler->message_type)
              << " at " << (void*)point.address.get() << "\n";
    */
    poll(plugins, message,
         [](auto &plugin, auto &message) { return plugin->handle_unsubscription(message.payload.point, false); });
}

void actor_base_t::on_unsubscription_external(message::unsubscription_external_t &message) noexcept {
    /*
    auto& point = message.payload.point;
    std::cout << "actor " << point.handler->actor_ptr.get() << " unsubscribed[e] from "
              << boost::core::demangle((const char*)point.handler->message_type)
              << " at " << (void*)point.address.get() << "\n";
    */
    poll(plugins, message,
         [](auto &plugin, auto &message) { return plugin->handle_unsubscription(message.payload.point, true); });
}

address_ptr_t actor_base_t::create_address() noexcept { return address_maker->create_address(); }

plugin_base_t *actor_base_t::get_plugin(const std::type_index &identity_) const noexcept {
    for (auto plugin : plugins) {
        if (plugin->identity() == identity_) {
            return plugin;
        }
    }
    return nullptr;
}

void actor_base_t::cancel_timer(request_id_t request_id) noexcept {
    assert(timers_map.find(request_id) != timers_map.end() && "request does exist");
    supervisor->do_cancel_timer(request_id);
}

void actor_base_t::on_timer_trigger(request_id_t request_id, bool cancelled) noexcept {
    auto it = timers_map.find(request_id);
    if (it != timers_map.end()) {
        it->second->trigger(cancelled);
        timers_map.erase(it);
    }
}

void actor_base_t::assign_shutdown_reason(extended_error_ptr_t reason) noexcept {
    if (!shutdown_reason) {
        shutdown_reason = std::move(reason);
    }
}

extended_error_ptr_t actor_base_t::make_error(const std::error_code &ec, const extended_error_ptr_t &next,
                                              const message_ptr_t &request) const noexcept {
    return ::make_error(identity, ec, next, request);
}

bool actor_base_t::on_unlink(const address_ptr_t &) noexcept { return true; }

bool actor_base_t::should_restart() const noexcept { return false; }
