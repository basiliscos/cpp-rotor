#pragma once

#include "rotor/supervisor.h"
#include "supervisor_config.h"
#include "system_context_base.h"
#include <boost/asio.hpp>

namespace rotor {
namespace asio {

namespace asio = boost::asio;

struct supervisor_asio_t : public supervisor_t {
  using timer_t = asio::deadline_timer;
  using supervisor_ptr_t = intrusive_ptr_t<supervisor_asio_t>;

  supervisor_asio_t(system_context_ptr_t system_context_,
                    const supervisor_config_t &config_);

  /*
  supervisor_asio_t(system_context_ptr_t system_context_,
               const supervisor_config_t &config_)
      : actor_base_t{*this},
        system_context{std::move(system_context_)},
        strand{system_context->io_context},
        config{config_},
        shutdown_timer{system_context->io_context} {

  }
  */
  virtual void start() noexcept;
  virtual void shutdown() noexcept;

  inline asio::io_context::strand &get_strand() noexcept { return strand; }

  system_context_ptr_t system_context;
  asio::io_context::strand strand;
  timer_t shutdown_timer;
  supervisor_config_t config;

private:
};

} // namespace asio
} // namespace rotor
