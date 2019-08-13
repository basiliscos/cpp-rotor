//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/asio/supervisor_asio.h"
#include "rotor/asio/forwarder.hpp"

using namespace rotor::asio;

supervisor_asio_t::supervisor_asio_t(supervisor_t *sup, system_context_ptr_t system_context_,
                                     const supervisor_config_t &config_)
    : supervisor_t{sup}, shutdown_timer{system_context_->io_context}, config{config_} {}

rotor::address_ptr_t supervisor_asio_t::make_address() noexcept { return instantiate_address(config.strand.get()); }

void supervisor_asio_t::start() noexcept { create_forwarder (&supervisor_asio_t::do_start)(); }

void supervisor_asio_t::shutdown() noexcept { create_forwarder (&supervisor_asio_t::do_shutdown)(); }

void supervisor_asio_t::start_shutdown_timer() noexcept {
    shutdown_timer.expires_from_now(config.shutdown_timeout);
    auto fwd = create_forwarder(&supervisor_t::on_shutdown_timer_trigger, &supervisor_asio_t::on_shutdown_timer_error);
    shutdown_timer.async_wait(std::move(fwd));
}

void supervisor_asio_t::on_shutdown_timer_error(const boost::system::error_code &ec) noexcept {
    if (ec != asio::error::operation_aborted) {
        get_asio_context().on_error(ec);
    }
}

void supervisor_asio_t::cancel_shutdown_timer() noexcept {
    boost::system::error_code ec;
    shutdown_timer.cancel(ec);
    if (ec) {
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
