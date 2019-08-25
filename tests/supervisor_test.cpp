//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "supervisor_test.h"
#include "catch.hpp"

using namespace rotor::test;
using namespace rotor;

supervisor_test_t::supervisor_test_t(supervisor_t *sup, const pt::time_duration &shutdown_timeout_,
                                     const void *locality_)
    : supervisor_t{sup, shutdown_timeout_}, locality{locality_} {}

address_ptr_t supervisor_test_t::make_address() noexcept { return instantiate_address(locality); }

void supervisor_test_t::start_timer(const pt::time_duration &, timer_id_t) noexcept {
    INFO("supervisor_test_t::start_timer()")
}
void supervisor_test_t::cancel_timer(timer_id_t) noexcept { INFO("supervisor_test_t::cancel_timer()") }

void supervisor_test_t::start() noexcept { INFO("supervisor_test_t::start()") }

void supervisor_test_t::shutdown() noexcept { INFO("supervisor_test_t::shutdown()") }

void supervisor_test_t::enqueue(message_ptr_t message) noexcept { effective_queue->emplace_back(std::move(message)); }
