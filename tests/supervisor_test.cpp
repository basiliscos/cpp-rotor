//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "supervisor_test.h"
#include "catch.hpp"
#include "cassert"

using namespace rotor::test;
using namespace rotor;

supervisor_test_t::supervisor_test_t(supervisor_t *sup, const supervisor_config_test_t& config_)
    : supervisor_t{sup, config_}, locality{config_.locality} {}

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

void supervisor_test_t::start() noexcept { INFO("supervisor_test_t::start()") }

supervisor_t::timer_id_t supervisor_test_t::pop_timer() noexcept {
    auto r = active_timers.front();
    active_timers.pop_front();
    return r;
}


void supervisor_test_t::shutdown() noexcept { INFO("supervisor_test_t::shutdown()") }

void supervisor_test_t::enqueue(message_ptr_t message) noexcept {
    get_leader().queue.emplace_back(std::move(message));
}
