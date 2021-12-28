//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/thread/system_context_thread.h"
#include "rotor/supervisor.h"
#include <chrono>

namespace rotor {
using namespace rotor::thread;

namespace {
namespace to {
struct state {};
struct queue {};
struct poll_duration {};
struct inbound_queue {};
struct on_timer_trigger {};
} // namespace to
} // namespace

template <> auto &supervisor_t::access<to::state>() noexcept { return state; }
template <> auto &supervisor_t::access<to::queue>() noexcept { return queue; }
template <> auto &supervisor_t::access<to::poll_duration>() noexcept { return poll_duration; }
template <> auto &supervisor_t::access<to::inbound_queue>() noexcept { return inbound_queue; }
template <>
inline auto rotor::actor_base_t::access<to::on_timer_trigger, request_id_t, bool>(request_id_t request_id,
                                                                                  bool cancelled) noexcept {
    on_timer_trigger(request_id, cancelled);
}

using time_units_t = std::chrono::microseconds;

system_context_thread_t::system_context_thread_t() noexcept { update_time(); }

void system_context_thread_t::run() noexcept {
    using std::chrono::duration_cast;
    auto &root_sup = *get_supervisor();
    auto condition = [&]() -> bool { return root_sup.access<to::state>() != state_t::SHUT_DOWN; };
    auto &queue = root_sup.access<to::queue>();
    auto &inbound = root_sup.access<to::inbound_queue>();
    auto &poll_duration = root_sup.access<to::poll_duration>();

    auto process = [&]() -> bool {
        message_base_t *ptr;
        if (inbound.pop(ptr)) {
            queue.emplace_back(ptr, false);
            return true;
        }
        return false;
    };

    auto delta = time_units_t{poll_duration.total_microseconds()};
    while (condition()) {
        root_sup.do_process();
        if (condition()) {
            using namespace std::chrono_literals;
            auto dealine = clock_t::now() + delta;
            if (!timer_nodes.empty()) {
                dealine = std::min(dealine, timer_nodes.front().deadline);
            }
            // fast stage, indirect spin-lock, cpu consuming
            while ((clock_t::now() < dealine) && !process()) {
            }
            if (queue.empty()) {
                std::unique_lock<std::mutex> lock(mutex);
                auto predicate = [&]() { return !inbound.empty(); };
                // wait notification, do not consume CPU
                auto next_timer_deadline = !timer_nodes.empty() ? timer_nodes.front().deadline : dealine + 1h;
                cv.wait_until(lock, next_timer_deadline, predicate);
            }
            update_time();
            root_sup.do_process();
        }
    }
}

void system_context_thread_t::check() noexcept {
    auto &root_sup = *get_supervisor();
    auto &queue = root_sup.access<to::queue>();
    auto &inbound = root_sup.access<to::inbound_queue>();
    message_base_t *ptr;
    while (inbound.pop(ptr)) {
        queue.emplace_back(ptr, false);
    }
    update_time();
}

void system_context_thread_t::update_time() noexcept {
    now = clock_t::now();
    auto it = timer_nodes.begin();
    while (it != timer_nodes.end() && it->deadline < now) {
        auto actor_ptr = it->handler->owner;
        actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(it->handler->request_id, false);
        it = timer_nodes.erase(it);
    }
}

void system_context_thread_t::start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept {
    if (intercepting)
        update_time();
    auto deadline = now + time_units_t{interval.total_microseconds()};
    auto it = timer_nodes.begin();
    for (; it != timer_nodes.end(); ++it) {
        if (deadline < it->deadline) {
            break;
        }
    }
    timer_nodes.insert(it, deadline_info_t{&handler, deadline});
}

void system_context_thread_t::cancel_timer(request_id_t timer_id) noexcept {
    if (intercepting)
        update_time();
    auto predicate = [&](auto &info) { return info.handler->request_id == timer_id; };
    auto it = std::find_if(timer_nodes.begin(), timer_nodes.end(), predicate);
    assert(it != timer_nodes.end() && "timer has been found");
    auto &actor_ptr = it->handler->owner;
    actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, true);
    timer_nodes.erase(it);
}

} // namespace rotor
