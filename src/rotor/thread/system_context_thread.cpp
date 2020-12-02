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

using mks_t = std::chrono::microseconds;
using clock_t = std::chrono::steady_clock;

void system_context_thread_t::run() noexcept {
    using std::chrono::duration_cast;
    auto &root_sup = *get_supervisor();
    while (root_sup.access<to::state>() != state_t::SHUT_DOWN) {
        root_sup.do_process();
        // no more messages
        std::unique_lock<std::mutex> lock(mutex);
        mks_t max_wait = timer_nodes.empty() ? mks_t::max() : duration_cast<mks_t>(timer_nodes.front().deadline - now);
        auto predicate = [&]() -> bool { return !inbound.empty(); };
        auto r = cv.wait_for(lock, max_wait, predicate);
        if (r) {
            auto &queue = root_sup.access<to::queue>();
            lock.lock();
            std::move(inbound.begin(), inbound.end(), std::back_inserter(queue));
            inbound.clear();
            lock.unlock();
        }
        update_time();
    }
    // post-mortem
    update_time();
    root_sup.do_process();
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
    auto deadline = now + std::chrono::microseconds{interval.total_microseconds()};
    auto it = timer_nodes.begin();
    for (; (it != timer_nodes.end()) && (it->deadline > deadline); ++it)
        ;
    timer_nodes.insert(it, deadline_info_t{&handler, deadline});
}

void system_context_thread_t::cancel_timer(request_id_t timer_id) noexcept {
    for (auto it = timer_nodes.begin(); it != timer_nodes.end(); ++it) {
        if (it->handler->request_id == timer_id) {
            auto &actor_ptr = it->handler->owner;
            actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, true);
            timer_nodes.erase(it);
            return;
        }
    }
    assert(0 && "timer has been found");
}

} // namespace rotor
