//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/ev/supervisor_ev.h"

using namespace rotor;
using namespace rotor::ev;

namespace rotor::ev {
namespace {
namespace to {
struct on_timer_trigger {};
struct timers_map {};
} // namespace to
} // namespace
} // namespace rotor::ev

namespace rotor {
template <>
inline auto rotor::actor_base_t::access<to::on_timer_trigger, request_id_t, bool>(request_id_t request_id,
                                                                                  bool cancelled) noexcept {
    on_timer_trigger(request_id, cancelled);
}

template <> inline auto &rotor::ev::supervisor_ev_t::access<to::timers_map>() noexcept { return timers_map; }

} // namespace rotor

void supervisor_ev_t::async_cb(struct ev_loop *, ev_async *w, int revents) noexcept {
    assert(revents & EV_ASYNC);
    (void)revents;
    auto *sup = static_cast<supervisor_ev_t *>(w->data);
    sup->on_async();
}

static void timer_cb(struct ev_loop *, ev_timer *w, int revents) noexcept {
    assert(revents & EV_TIMER);
    (void)revents;
    auto *sup = static_cast<supervisor_ev_t *>(w->data);
    intrusive_ptr_release(sup);
    auto timer = static_cast<supervisor_ev_t::timer_t *>(w);
    auto timer_id = timer->handler->request_id;
    auto &timers_map = sup->access<to::timers_map>();
    auto it = timers_map.find(timer_id);
    if (it != timers_map.end()) {
        auto actor_ptr = timer->handler->owner;
        actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, false);
        timers_map.erase(it);
        sup->do_process();
    }
}

supervisor_ev_t::supervisor_ev_t(supervisor_config_ev_t &config_)
    : supervisor_t{config_}, loop{config_.loop}, loop_ownership{config_.loop_ownership}, pending{false} {
    ev_async_init(&async_watcher, async_cb);
}

void supervisor_ev_t::do_initialize(system_context_t *ctx) noexcept {
    async_watcher.data = this;
    ev_async_start(loop, &async_watcher);
    supervisor_t::do_initialize(ctx);
}

void supervisor_ev_t::enqueue(rotor::message_ptr_t message) noexcept {
    bool ok{false};
    try {
        auto leader = static_cast<supervisor_ev_t *>(locality_leader);
        auto &inbound = leader->inbound;
        std::lock_guard<std::mutex> lock(leader->inbound_mutex);
        if (leader->state < state_t::SHUT_DOWN) {
            if (!leader->pending) {
                // async events are "compressed" by EV. Need to do only once
                intrusive_ptr_add_ref(this);
            }
            inbound.emplace_back(std::move(message));
            ok = true;
        }
    } catch (const std::system_error &err) {
        context->on_error(make_error(err.code()));
    }

    if (ok) {
        ev_async_send(loop, &async_watcher);
    }
}

void supervisor_ev_t::start() noexcept {
    bool ok{false};
    try {
        auto leader = static_cast<supervisor_ev_t *>(locality_leader);
        std::lock_guard<std::mutex> lock(leader->inbound_mutex);
        if (!leader->pending) {
            intrusive_ptr_add_ref(leader);
            leader->pending = true;
        }
        ok = true;
    } catch (const std::system_error &err) {
        context->on_error(make_error(err.code()));
    }

    if (ok) {
        ev_async_send(loop, &async_watcher);
    }
}

void supervisor_ev_t::shutdown_finish() noexcept {
    supervisor_t::shutdown_finish();
    ev_async_stop(loop, &async_watcher);
    move_inbound_queue();
}

void supervisor_ev_t::shutdown() noexcept {
    auto &sup_addr = supervisor->get_address();
    auto ec = make_error_code(shutdown_code_t::normal);
    auto reason = make_error(ec);
    supervisor->enqueue(make_message<payload::shutdown_trigger_t>(sup_addr, address, reason));
}

void supervisor_ev_t::do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept {
    auto timer = std::make_unique<timer_t>();
    auto timer_ptr = timer.get();
    ev_tstamp ev_timeout = static_cast<ev_tstamp>(interval.total_nanoseconds()) / 1000000000;
    ev_timer_init(timer_ptr, timer_cb, ev_timeout, 0);
    timer_ptr->handler = &handler;
    timer_ptr->data = this;

    ev_timer_start(loop, timer_ptr);
    intrusive_ptr_add_ref(this);
    timers_map.emplace(handler.request_id, std::move(timer));
}

void supervisor_ev_t::do_cancel_timer(request_id_t timer_id) noexcept {
    auto it = timers_map.find(timer_id);
    if (it != timers_map.end()) {
        auto &timer = timers_map.at(timer_id);
        ev_timer_stop(loop, timer.get());
        auto actor_ptr = it->second->handler->owner;
        actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, true);
        timers_map.erase(it);
        intrusive_ptr_release(this);
    }
}

void supervisor_ev_t::on_async() noexcept {
    auto leader = static_cast<supervisor_ev_t *>(locality_leader);
    intrusive_ptr_release(leader);
    if (move_inbound_queue()) {
        do_process();
    }
}

supervisor_ev_t::~supervisor_ev_t() {
    if (loop_ownership) {
        ev_loop_destroy(loop);
    }
}

bool supervisor_ev_t::move_inbound_queue() noexcept {
    bool ok{false};
    auto leader = static_cast<supervisor_ev_t *>(locality_leader);
    try {
        std::lock_guard<std::mutex> lock(leader->inbound_mutex);
        auto &inbound = leader->inbound;
        auto &queue = leader->queue;
        std::move(inbound.begin(), inbound.end(), std::back_inserter(queue));
        inbound.clear();
        leader->pending = false;
        ok = true;
    } catch (const std::system_error &err) {
        context->on_error(make_error(err.code()));
    }
    return ok;
}
