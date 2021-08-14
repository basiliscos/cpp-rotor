//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/asio/supervisor_asio.h"
#include "rotor/asio/forwarder.hpp"

using namespace rotor::asio;
using namespace rotor;

namespace rotor::asio {
namespace {
namespace to {
struct on_timer_trigger {};
} // namespace to
} // namespace
} // namespace rotor::asio

namespace rotor {
template <>
inline auto rotor::actor_base_t::access<to::on_timer_trigger, request_id_t, bool>(request_id_t request_id,
                                                                                  bool cancelled) noexcept {
    on_timer_trigger(request_id, cancelled);
}
} // namespace rotor

supervisor_asio_t::supervisor_asio_t(supervisor_config_asio_t &config_)
    : supervisor_t{config_}, strand{config_.strand} {
    if (config_.guard_context) {
        guard = std::make_unique<guard_t>(asio::make_work_guard(strand->context()));
    }
}

rotor::address_ptr_t supervisor_asio_t::make_address() noexcept { return instantiate_address(strand.get()); }

void supervisor_asio_t::start() noexcept { create_forwarder (&supervisor_asio_t::do_process)(); }

void supervisor_asio_t::shutdown() noexcept { create_forwarder (&supervisor_asio_t::invoke_shutdown)(); }

void supervisor_asio_t::do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept {
    auto timer = std::make_unique<supervisor_asio_t::timer_t>(&handler, strand->context());
    timer->expires_from_now(interval);

    intrusive_ptr_t<supervisor_asio_t> self(this);
    request_id_t timer_id = handler.request_id;
    timer->async_wait([self = std::move(self), timer_id = timer_id](const boost::system::error_code &ec) {
        auto &strand = self->get_strand();
        if (!ec) {
            asio::defer(strand, [self = std::move(self), timer_id = timer_id]() {
                auto &sup = *self;
                auto &timers_map = sup.timers_map;
                auto it = timers_map.find(timer_id);
                if (it != timers_map.end()) {
                    auto &actor_ptr = it->second->handler->owner;
                    actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, false);
                    timers_map.erase(it);
                    sup.do_process();
                }
            });
        }
    });
    timers_map.emplace(timer_id, std::move(timer));
}

void supervisor_asio_t::do_cancel_timer(request_id_t timer_id) noexcept {
    auto it = timers_map.find(timer_id);
    assert(it != timers_map.end());
    auto &timer = it->second;
    boost::system::error_code ec;
    timer->cancel(ec);

    auto &actor_ptr = it->second->handler->owner;
    actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, true);
    timers_map.erase(it);
    // ignore the possible error, caused the case when timer is not cancelleable
    // if (ec) { ... }
}

void supervisor_asio_t::enqueue(rotor::message_ptr_t message) noexcept {
    auto leader = static_cast<supervisor_asio_t *>(locality_leader);
    auto &inbound = leader->inbound_queue;
    inbound.push(message.detach());

    auto actor_ptr = supervisor_ptr_t(this);
    asio::defer(get_strand(), [actor = std::move(actor_ptr)]() mutable {
        auto &sup = *actor;
        sup.do_process();
    });
}

void supervisor_asio_t::shutdown_finish() noexcept {
    if (guard)
        guard.reset();
    supervisor_t::shutdown_finish();
}

void supervisor_asio_t::invoke_shutdown() noexcept {
    auto ec = make_error_code(shutdown_code_t::normal);
    auto reason = make_error(ec);
    do_shutdown(reason);
}

void supervisor_asio_t::do_process() noexcept {
    using clock_t = std::chrono::steady_clock;
    using time_units_t = std::chrono::microseconds;

    bool ok = false;
    message_base_t *ptr;
    while (inbound_queue.pop(ptr)) {
        queue.emplace_back(ptr);
        intrusive_ptr_release(ptr);
        ok = true;
    }
    if (!queue.empty()) {
        supervisor_t::do_process();
    }

    if (ok) {
        auto deadline = clock_t::now() + time_units_t{poll_duration.total_microseconds()};
        while (clock_t::now() < deadline && queue.empty()) {
            message_base_t *ptr;
            while (inbound_queue.pop(ptr)) {
                queue.emplace_back(ptr);
                intrusive_ptr_release(ptr);
            }
        }
        if (!queue.empty()) {
            supervisor_t::do_process();
        }
    }
}
