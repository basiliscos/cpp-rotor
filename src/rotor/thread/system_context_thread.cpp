#include "rotor/thread/system_context_thread.h"
#include "rotor/supervisor.h"
#include <chrono>

namespace rotor {
using namespace rotor::thread;

namespace {
namespace to {
struct state {};
struct queue {};
struct on_timer_trigger {};
} // namespace to
} // namespace

template <> auto &supervisor_t::access<to::state>() noexcept { return state; }
template <> auto &supervisor_t::access<to::queue>() noexcept { return queue; }
template <>
inline auto rotor::actor_base_t::access<to::on_timer_trigger, request_id_t, bool>(request_id_t request_id,
                                                                                  bool cancelled) noexcept {
    on_timer_trigger(request_id, cancelled);
}

using clock_t = std::chrono::steady_clock;

system_context_thread_t::system_context_thread_t() noexcept { update_time(); }

void system_context_thread_t::run() noexcept {
    using time_unit_t = clock_t::duration;
    using std::chrono::duration_cast;
    auto &root_sup = *get_supervisor();
    auto condition = [&]() -> bool { return root_sup.access<to::state>() != state_t::SHUT_DOWN; };
    while (condition()) {
        root_sup.do_process();
        if (condition()) {
            std::unique_lock<std::mutex> lock(mutex);
            // / 16 is because sanitizer complains on overflow. No idea why, but seems enough
            // time_unit_t max_wait = timer_nodes.empty() ? time_unit_t::max() / 16 : (timer_nodes.front().deadline -
            // now);
            auto predicate = [&]() -> bool { return !inbound.empty(); };
            bool r = false;
            if (!timer_nodes.empty()) {
                // printf("waiting until ...: %lu, req = %lu\n",
                // timer_nodes.begin()->deadline.time_since_epoch().count(), timer_nodes.begin()->handler->request_id);
                r = cv.wait_until(lock, timer_nodes.begin()->deadline, predicate);
            } else {
                cv.wait(lock, predicate);
                r = true;
            }
            if (r) {
                auto &queue = root_sup.access<to::queue>();
                std::move(inbound.begin(), inbound.end(), std::back_inserter(queue));
                inbound.clear();
            }
            lock.unlock();
            update_time();
            root_sup.do_process();
        }
    }
}

void system_context_thread_t::update_time() noexcept {
    now = clock_t::now();
    // printf("update_time to : %lu\n", now.time_since_epoch().count());
    auto it = timer_nodes.begin();
    while (it != timer_nodes.end() && it->deadline < now) {
        auto actor_ptr = it->handler->owner;
        // printf("triggered deadline : %lu\n", it->deadline.time_since_epoch().count());
        actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(it->handler->request_id, false);
        it = timer_nodes.erase(it);
    }
}

void system_context_thread_t::start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept {
    if (intercepting)
        update_time();
    auto deadline = now + std::chrono::microseconds{interval.total_microseconds()};
    // printf("started_timer, deadline: %lu, req = %lu\n", deadline.time_since_epoch().count(), handler.request_id);
    auto it = timer_nodes.begin();
    for (; it != timer_nodes.end(); ++it) {
        if (deadline < it->deadline) {
            break;
        }
    }
    timer_nodes.insert(it, deadline_info_t{&handler, deadline});
    // printf("started_timer, first node deadline: %lu\n", timer_nodes.begin()->deadline);
}

void system_context_thread_t::cancel_timer(request_id_t timer_id) noexcept {
    if (intercepting)
        update_time();
    for (auto it = timer_nodes.begin(); it != timer_nodes.end(); ++it) {
        if (it->handler->request_id == timer_id) {
            auto &actor_ptr = it->handler->owner;
            actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, true);
            // printf("triggered cancel, deadline : %lu, timer_id = %lu, \n", it->deadline.time_since_epoch().count(),
            // timer_id);
            timer_nodes.erase(it);
            // if (timer_nodes.size()) {
            //    printf("cancel_timer, first node deadline: %lu\n", timer_nodes.begin()->deadline);
            //}
            return;
        }
    }
    assert(0 && "timer has been found");
}

} // namespace rotor
