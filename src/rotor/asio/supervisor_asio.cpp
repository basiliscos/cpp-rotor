//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/asio/supervisor_asio.h"
#include "rotor/asio/forwarder.hpp"

using namespace rotor::asio;

supervisor_asio_t::supervisor_asio_t(const supervisor_config_asio_t &config_)
    : supervisor_t{config_}, strand{config_.strand} {}

rotor::address_ptr_t supervisor_asio_t::make_address() noexcept { return instantiate_address(strand.get()); }

void supervisor_asio_t::start() noexcept { create_forwarder (&supervisor_asio_t::do_process)(); }

void supervisor_asio_t::shutdown() noexcept { create_forwarder (&supervisor_asio_t::do_shutdown)(); }

void supervisor_asio_t::start_timer(const rotor::pt::time_duration &timeout, request_id_t timer_id) noexcept {
    auto timer = std::make_unique<timer_t>(timer_id, get_asio_context().get_io_context());
    timer->expires_from_now(timeout);

    intrusive_ptr_t<supervisor_asio_t> self(this);
    timer->async_wait([self = std::move(self), timer_id = timer_id](const boost::system::error_code &ec) {
        auto &strand = self->get_strand();
        if (ec) {
            asio::defer(strand, [self = std::move(self), timer_id = timer_id, ec = ec]() {
                auto &sup = *self;
                sup.timers_map.erase(timer_id);
                sup.on_timer_error(timer_id, ec);
                sup.do_process();
            });
        } else {
            asio::defer(strand, [self = std::move(self), timer_id = timer_id]() {
                auto &sup = *self;
                sup.on_timer_trigger(timer_id);
                sup.timers_map.erase(timer_id);
                sup.do_process();
            });
        }
    });
    timers_map.emplace(timer_id, std::move(timer));
}

void supervisor_asio_t::cancel_timer(request_id_t timer_id) noexcept {
    auto &timer = timers_map.at(timer_id);
    boost::system::error_code ec;
    timer->cancel(ec);
    if (ec) {
        get_asio_context().on_error(ec);
    }
    timers_map.erase(timer_id);
}

void supervisor_asio_t::on_timer_error(request_id_t, const boost::system::error_code &ec) noexcept {
    if (ec != asio::error::operation_aborted) {
        get_asio_context().on_error(ec);
    }
}

void supervisor_asio_t::enqueue(rotor::message_ptr_t message) noexcept {
    auto actor_ptr = supervisor_ptr_t(this);
    // std::cout << "deferring on " << this << ", stopped : " << strand.get_io_context().stopped() << "\n";
    asio::defer(get_strand(), [actor = std::move(actor_ptr), message = std::move(message)]() mutable {
        auto &sup = *actor;
        // std::cout << "deferred processing on" << &sup << "\n";
        // sup.enqueue(std::move(message));
        sup.put(std::move(message));
        sup.do_process();
    });
}
