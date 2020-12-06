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

system_context_thread_t::system_context_thread_t() noexcept { update_time(); }

void system_context_thread_t::run() noexcept {
    using std::chrono::duration_cast;
    auto &root_sup = *get_supervisor();
    auto condition = [&]() -> bool { return root_sup.access<to::state>() != state_t::SHUT_DOWN; };
    while (condition()) {
        root_sup.do_process();
        if (condition()) {
            std::unique_lock<std::mutex> lock(mutex);
            auto predicate = [&]() -> bool { return !inbound.empty(); };
            bool r = false;
            if (!timer_nodes.empty()) {
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
    auto deadline = now + std::chrono::microseconds{interval.total_microseconds()};
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
    auto predicate = [&](auto& info) { return info.handler->request_id == timer_id; };
    auto it = std::find_if(timer_nodes.begin(), timer_nodes.end(), predicate);
    assert(it != timer_nodes.end() && "timer has been found");
    auto &actor_ptr = it->handler->owner;
    actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, true);
    timer_nodes.erase(it);
}

} // namespace rotor
