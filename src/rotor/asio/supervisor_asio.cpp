//
// Copyright (c) 2019-2025 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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
    using namespace asio::chrono;
    auto timer = std::make_unique<timer_t>(&handler, strand->context());
    auto micro_seconds = microseconds(interval.total_microseconds());
    timer->expires_after(duration_cast<timer_t::duration>(micro_seconds));

    intrusive_ptr_t<supervisor_asio_t> self(this);
    request_id_t timer_id = handler.request_id;
    timer->async_wait([self = self, timer_id = timer_id](const boost::system::error_code &ec) {
        auto &strand = self->get_strand();
        if (!ec) {
            asio::defer(strand, [self = self, timer_id = timer_id]() {
                auto &sup = *self;
                auto &timers_map = sup.timers_map;
                auto it = timers_map.find(timer_id);
                if (it != timers_map.end()) {
                    auto actor_ptr = it->second->handler->owner;
                    actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, false);
                    timers_map.erase(timer_id);
                    sup.do_process();
                }
            });
        }
    });
    timers_map.emplace(timer_id, std::move(timer));
}

void supervisor_asio_t::do_cancel_timer(request_id_t timer_id) noexcept {
    auto &timer = timers_map.at(timer_id);
    boost::system::error_code ec;
    timer->cancel(ec);

    auto &actor_ptr = timer->handler->owner;
    actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, true);
    timers_map.erase(timer_id);
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

    auto leader = static_cast<supervisor_asio_t *>(locality_leader);
    auto &inbound = leader->inbound_queue;
    auto &leader_queue = leader->queue;
    auto enqueued_messages = size_t{0};
    message_base_t *ptr;
    while (inbound.pop(ptr)) {
        leader_queue.emplace_back(ptr, false);
    }
    if (!leader_queue.empty()) {
        enqueued_messages = supervisor_t::do_process();
    }

    if (enqueued_messages) {
        auto deadline = clock_t::now() + time_units_t{poll_duration.total_microseconds()};
        while (clock_t::now() < deadline && leader_queue.empty()) {
            while (inbound.pop(ptr)) {
                leader_queue.emplace_back(ptr, false);
            }
        }
        if (!leader_queue.empty()) {
            supervisor_t::do_process();
        }
    }
}
