#pragma once

#include "rotor/supervisor.h"
#include "supervisor_config.h"
#include "system_context_asio.h"
#include <boost/asio.hpp>

namespace rotor {
namespace asio {

namespace asio = boost::asio;
namespace sys = boost::system;

struct supervisor_asio_t : public supervisor_t {
  using timer_t = asio::deadline_timer;
  using supervisor_ptr_t = intrusive_ptr_t<supervisor_asio_t>;

  supervisor_asio_t(system_context_ptr_t system_context_,
                    const supervisor_config_t &config_);

  virtual void start() noexcept override;
  virtual void shutdown() noexcept override;
  virtual void start_shutdown_timer() noexcept override;
  virtual void cancel_shutdown_timer() noexcept override;
  virtual void on_shutdown_timer_error(const sys::error_code &ec) noexcept;

  template <typename Actor, typename... Args>
  intrusive_ptr_t<Actor> create_actor(Args... args) {
    return supervisor_t::create_actor<Actor>(std::forward<Args>(args)...);
  }

  inline asio::io_context::strand &get_strand() noexcept { return strand; }

  system_context_ptr_t system_context;
  asio::io_context::strand strand;
  timer_t shutdown_timer;
  supervisor_config_t config;
};

} // namespace asio
} // namespace rotor
