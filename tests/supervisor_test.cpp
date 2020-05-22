//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "supervisor_test.h"
#include "actor_test.h"
#include "catch.hpp"
#include "cassert"

using namespace rotor::test;
using namespace rotor;

supervisor_test_t::supervisor_test_t(supervisor_config_test_t &config_)
    : supervisor_t{config_}, locality{config_.locality} {}

address_ptr_t supervisor_test_t::make_address() noexcept { return instantiate_address(locality); }

void supervisor_test_t::start_timer(const pt::time_duration &, timer_id_t timer_id) noexcept {
    active_timers.emplace_back(timer_id);
}

void supervisor_test_t::cancel_timer(timer_id_t timer_id) noexcept {
    auto it = active_timers.begin();
    while (it != active_timers.end()) {
        if (*it == timer_id) {
            active_timers.erase(it);
            return;
        } else {
            ++it;
        }
    }
    assert(0 && "should not happen");
}


subscription_points_t& supervisor_test_t::get_points() noexcept {
    for(auto p: plugins) {
        if (p->identity() == internal::lifetime_plugin_t::class_identity) {
            auto plugin = static_cast<internal::lifetime_plugin_t*>(p);
            return plugin->points;
        }
    }
    std::abort();
}

//void supervisor_test_t::start() noexcept {INFO("supervisor_test_t::start()")}

supervisor_t::timer_id_t supervisor_test_t::get_timer(std::size_t index) noexcept {
    auto it = active_timers.begin();
    for (std::size_t i = 0; i < index; ++i) {
        ++it;
    }
    return *it;
}

plugin_t* supervisor_test_t::get_plugin(const void* identity) const noexcept {
    return actor_test_t::get_plugin(plugins, identity);
}


//void supervisor_test_t::shutdown() noexcept { INFO("supervisor_test_t::shutdown()") }

void supervisor_test_t::enqueue(message_ptr_t message) noexcept { get_leader().queue.emplace_back(std::move(message)); }

pt::time_duration rotor::test::default_timeout{pt::milliseconds{1}};

