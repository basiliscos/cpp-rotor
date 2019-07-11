#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "supervisor_config.h"
#include "system_context_asio.h"
#include "forwarder.hpp"
#include <boost/asio.hpp>

namespace rotor {
namespace asio {

namespace asio = boost::asio;
namespace sys = boost::system;

template <typename Actor, typename Handler, typename ErrHandler> struct forwarder_t;

struct supervisor_asio_t : public supervisor_t {
    using timer_t = asio::deadline_timer;
    using supervisor_ptr_t = intrusive_ptr_t<supervisor_asio_t>;

    supervisor_asio_t(supervisor_t *sup, system_context_ptr_t system_context_, const supervisor_config_t &config);

    virtual void start() noexcept override;
    virtual void shutdown() noexcept override;
    virtual void enqueue(message_ptr_t message) noexcept override;
    virtual void start_shutdown_timer() noexcept override;
    virtual void cancel_shutdown_timer() noexcept override;
    virtual void on_shutdown_timer_error(const sys::error_code &ec) noexcept;

    inline asio::io_context::strand &get_strand() noexcept { return strand; }

    template <typename Actor, typename... Args> intrusive_ptr_t<Actor> create_actor(Args... args) {
        return make_actor<Actor>(*this, std::forward<Args>(args)...);
    }

    template <typename Handler, typename ErrHandler>
    auto create_forwarder(Handler &&handler, ErrHandler &&err_handler) {
        return forwarder_t{*this, std::move(handler), std::move(err_handler)};
    }

    template <typename Handler> auto create_forwarder(Handler &&handler) {
        return forwarder_t{*this, std::move(handler)};
    }

    inline system_context_asio_t &get_asio_context() noexcept { return static_cast<system_context_asio_t &>(*context); }

    asio::io_context::strand strand;
    timer_t shutdown_timer;
    supervisor_config_t config;
};

template <typename Actor> inline boost::asio::io_context::strand &get_strand(Actor &actor) {
    return actor.get_strand();
}

} // namespace asio
} // namespace rotor
