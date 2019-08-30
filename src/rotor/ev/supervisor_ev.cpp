//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <algorithm>
#include "rotor/ev/supervisor_ev.h"
#include <iostream>

using namespace rotor::ev;

void supervisor_ev_shutdown_t::cleanup() noexcept {
    auto &sup = static_cast<supervisor_ev_t &>(actor);
    ev_async_stop(sup.config.loop, &sup.async_watcher);
}

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
    auto timer = static_cast<supervisor_ev_t::timer_t *>(w);
    sup->on_timer_trigger(timer->timer_id);
    sup->do_process();
}

supervisor_ev_t::supervisor_ev_t(supervisor_ev_t *parent_, const pt::time_duration &shutdown_timeout_,
                                 const supervisor_config_t &config_)
    : supervisor_t{parent_, shutdown_timeout_}, config{config_.loop, config_.loop_ownership}, pending{false} {
    ev_async_init(&async_watcher, async_cb);

    async_watcher.data = this;

    ev_async_start(config.loop, &async_watcher);
}

void supervisor_ev_t::enqueue(rotor::message_ptr_t message) noexcept {
    bool ok{false};
    try {
        std::lock_guard<std::mutex> lock(inbound_mutex);
        if (!pending) {
            // async events are "compressed" by EV. Need to do only once
            intrusive_ptr_add_ref(this);
        }
        inbound.emplace_back(std::move(message));
        ok = true;
    } catch (const std::system_error &err) {
        context->on_error(err.code());
    }

    if (ok) {
        ev_async_send(config.loop, &async_watcher);
    }
}

void supervisor_ev_t::start() noexcept {
    bool ok{false};
    try {
        std::lock_guard<std::mutex> lock(inbound_mutex);
        if (!pending) {
            pending = true;
            intrusive_ptr_add_ref(this);
        }
        ok = true;
    } catch (const std::system_error &err) {
        context->on_error(err.code());
    }

    if (ok) {
        ev_async_send(config.loop, &async_watcher);
    }
}

void supervisor_ev_t::shutdown_initiate() noexcept {
    behaviour = std::make_unique<supervisor_ev_shutdown_t>(*this);
    behaviour->init();
}

void supervisor_ev_t::shutdown() noexcept {
    supervisor.enqueue(make_message<payload::shutdown_trigger_t>(supervisor.get_address(), address));
}

void supervisor_ev_t::start_timer(const pt::time_duration &timeout, timer_id_t timer_id) noexcept {
    auto timer = std::make_unique<timer_t>();
    auto timer_ptr = timer.get();
    ev_tstamp ev_timeout = static_cast<ev_tstamp>(timeout.total_nanoseconds()) / 1000000000;
    ev_timer_init(timer_ptr, timer_cb, ev_timeout, 0);
    timer_ptr->timer_id = timer_id;
    timer_ptr->data = this;

    ev_timer_start(config.loop, timer_ptr);
    intrusive_ptr_add_ref(this);
    timers_map.emplace(timer_id, std::move(timer));
}

void supervisor_ev_t::cancel_timer(timer_id_t timer_id) noexcept {
    auto &timer = timers_map.at(timer_id);
    ev_timer_stop(config.loop, timer.get());
    timers_map.erase(timer_id);
    intrusive_ptr_release(this);
}

void supervisor_ev_t::on_timer_trigger(timer_id_t timer_id) noexcept {
    intrusive_ptr_release(this);
    timers_map.erase(timer_id);
    supervisor_t::on_timer_trigger(timer_id);
}

void supervisor_ev_t::on_async() noexcept {
    bool ok{false};
    try {
        std::lock_guard<std::mutex> lock(inbound_mutex);
        std::move(inbound.begin(), inbound.end(), std::back_inserter(*effective_queue));
        inbound.clear();
        pending = false;
        intrusive_ptr_release(this);
        ok = true;
    } catch (const std::system_error &err) {
        context->on_error(err.code());
    }

    if (ok) {
        do_process();
    }
}

supervisor_ev_t::~supervisor_ev_t() {
    if (config.loop_ownership) {
        ev_loop_destroy(config.loop);
    }
}
