//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "supervisor_test.h"
#include "access.h"
#include "catch.hpp"
#include "cassert"

using namespace rotor::test;
using namespace rotor;

supervisor_test_t::supervisor_test_t(supervisor_config_test_t &config_)
    : supervisor_t{config_}, locality{config_.locality}, configurer{std::move(config_.configurer)},
      interceptor{std::move(config_.interceptor)} {
    supervisor_t *root_sup = this;
    if (!locality) {
        while (root_sup->access<test::to::parent_supervisor>()) {
            root_sup = root_sup->access<test::to::parent_supervisor>();
        }
        locality = root_sup;
    }
}

supervisor_test_t::~supervisor_test_t() { printf("~supervisor_test_t, %p(%p)\n", (void *)this, (void *)address.get()); }

address_ptr_t supervisor_test_t::make_address() noexcept { return instantiate_address(locality); }

void supervisor_test_t::do_start_timer(const pt::time_duration &, timer_handler_base_t &handler) noexcept {
    printf("starting timer %zu (%p)\n", handler.request_id, (void *)this);
    active_timers.emplace_back(&handler);
}

void supervisor_test_t::do_cancel_timer(request_id_t timer_id) noexcept {
    printf("cancelling timer %zu (%p)\n", timer_id, (void *)this);
    auto it = active_timers.begin();
    while (it != active_timers.end()) {
        auto &handler = *it;
        if (handler->request_id == timer_id) {
            auto &actor_ptr = handler->owner;
            actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, true);
            active_timers.erase(it);
            return;
        } else {
            ++it;
        }
    }
    assert(0 && "should not happen");
}

void supervisor_test_t::do_invoke_timer(request_id_t timer_id) noexcept {
    printf("invoking timer %zu (%p)\n", timer_id, (void *)this);
    auto predicate = [&](auto &handler) { return handler->request_id == timer_id; };
    auto it = std::find_if(active_timers.begin(), active_timers.end(), predicate);
    assert(it != active_timers.end());
    auto &handler = *it;
    auto &actor_ptr = handler->owner;
    actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, false);
    active_timers.erase(it);
}

subscription_container_t &supervisor_test_t::get_points() noexcept {
    auto plugin = get_plugin(plugin::lifetime_plugin_t::class_identity);
    return static_cast<plugin::lifetime_plugin_t *>(plugin)->access<to::points>();
}

request_id_t supervisor_test_t::get_timer(std::size_t index) noexcept {
    auto it = active_timers.begin();
    for (std::size_t i = 0; i < index; ++i) {
        ++it;
    }
    return (*it)->request_id;
}

void supervisor_test_t::enqueue(message_ptr_t message) noexcept { get_leader().queue.emplace_back(std::move(message)); }

pt::time_duration rotor::test::default_timeout{pt::milliseconds{1}};

size_t supervisor_test_t::get_children_count() noexcept { return manager->access<to::actors_map>().size(); }

supervisor_test_t &supervisor_test_t::get_leader() {
    return *static_cast<supervisor_test_t *>(access<to::locality_leader>());
}

void supervisor_test_t::configure(plugin::plugin_base_t &plugin) noexcept {
    supervisor_t::configure(plugin);
    if (configurer) {
        configurer(*this, plugin);
    }
}

void supervisor_test_t::intercept(message_ptr_t &message, const void *tag,
                                  const continuation_t &continuation) noexcept {
    if (interceptor) {
        return interceptor(message, tag, continuation);
    }
    continuation();
}

// let trigger memory leaks
system_test_context_t::~system_test_context_t() {
    auto &sup = access<to::supervisor>();
    if (sup) {
        auto &queue = sup->access<to::queue>();
        auto &inbound = sup->access<to::inbound_queue>();
        while (!queue.empty()) {
            inbound.push(queue.front().detach());
            queue.pop_front();
        }
    }
}
