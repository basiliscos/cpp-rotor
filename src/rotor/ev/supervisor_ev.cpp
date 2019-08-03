//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <algorithm>
#include "rotor/ev/supervisor_ev.h"
#include <iostream>

using namespace rotor::ev;

void supervisor_ev_t::async_cb(EV_P_ ev_async *w, int revents) noexcept {
    assert(revents & EV_ASYNC);
    auto *sup = static_cast<supervisor_ev_t *>(w->data);
    sup->on_async();
    intrusive_ptr_release(sup);
}

static void timer_cb(EV_P_ ev_timer *w, int revents) noexcept {
    assert(revents & EV_TIMER);
    auto *sup = static_cast<supervisor_ev_t *>(w->data);
    sup->on_shutdown_timer_trigger();
    intrusive_ptr_release(sup);
}

supervisor_ev_t::supervisor_ev_t(supervisor_ev_t *parent_, const supervisor_config_t &config_)
    : supervisor_t{parent_}, config{config_.loop, config_.loop_ownership, config_.shutdown_timeout} {
    ev_async_init(&async_watcher, async_cb);
    ev_timer_init(&shutdown_watcher, timer_cb, config.shutdown_timeout, 0);

    async_watcher.data = this;
    shutdown_watcher.data = this;

    ev_async_start(config.loop, &async_watcher);
}

void supervisor_ev_t::enqueue(rotor::message_ptr_t message) noexcept {
    bool ok{false};
    try {
        std::lock_guard<std::mutex> lock(inbound_mutex);
        if (inbound.empty()) {
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

void supervisor_ev_t::start() noexcept { enqueue(make_message<payload::start_actor_t>(address)); }

void supervisor_ev_t::shutdown() noexcept {
    supervisor.enqueue(make_message<payload::shutdown_request_t>(supervisor.get_address(), address));
}

void supervisor_ev_t::start_shutdown_timer() noexcept {
    intrusive_ptr_add_ref(this);
    ev_timer_start(config.loop, &shutdown_watcher);
}

void supervisor_ev_t::cancel_shutdown_timer() noexcept {
    ev_timer_stop(config.loop, &shutdown_watcher);
    intrusive_ptr_release(this);
}

void supervisor_ev_t::on_async() noexcept {
    bool ok{false};
    try {
        std::lock_guard<std::mutex> lock(inbound_mutex);
        std::move(inbound.begin(), inbound.end(), std::back_inserter(outbound));
        inbound.clear();
        ok = true;
    } catch (const std::system_error &err) {
        context->on_error(err.code());
    }

    if (ok) {
        do_process();
    }
}

void supervisor_ev_t::confirm_shutdown() noexcept {
    supervisor_t::confirm_shutdown();
    ev_async_stop(config.loop, &async_watcher);
}

supervisor_ev_t::~supervisor_ev_t() {
    if (config.loop_ownership) {
        ev_loop_destroy(config.loop);
    }
}
