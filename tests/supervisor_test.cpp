//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "supervisor_test.h"
#include "catch.hpp"

using namespace rotor::test;
using namespace rotor;

supervisor_test_t::supervisor_test_t(supervisor_t *sup, const supervisor_config_test_t& config_)
    : supervisor_t{sup, config_}, locality{config_.locality} {}

address_ptr_t supervisor_test_t::make_address() noexcept { return instantiate_address(locality); }

void supervisor_test_t::start_timer(const pt::time_duration &, timer_id_t timer_id) noexcept {
    active_timers.insert(timer_id);
}
void supervisor_test_t::cancel_timer(timer_id_t timer_id) noexcept {
    auto it = active_timers.find(timer_id);
    active_timers.erase(it);
}

void supervisor_test_t::start() noexcept { INFO("supervisor_test_t::start()") }

void supervisor_test_t::shutdown() noexcept { INFO("supervisor_test_t::shutdown()") }

void supervisor_test_t::enqueue(message_ptr_t message) noexcept { effective_queue->emplace_back(std::move(message)); }
