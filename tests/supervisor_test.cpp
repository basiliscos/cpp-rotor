#include "supervisor_test.h"
#include "catch.hpp"

using namespace rotor::test;

supervisor_test_t::supervisor_test_t(supervisor_t *sup): supervisor_t{sup} {

}

void supervisor_test_t::start_shutdown_timer() noexcept {
    INFO("supervisor_test_t::start_shutdown_timer()")
}

void supervisor_test_t::cancel_shutdown_timer() noexcept {
    INFO("supervisor_test_t::cancel_shutdown_timer()")
}

void supervisor_test_t::start() noexcept {
    INFO("supervisor_test_t::start()")
}

void supervisor_test_t::shutdown() noexcept {
    INFO("supervisor_test_t::shutdown()")
}

void supervisor_test_t::enqueue(message_ptr_t message) noexcept {
    outbound.emplace_back(std::move(message));
}
