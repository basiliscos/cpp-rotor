#pragma once

#include "rotor/actor_base.h"
#include "rotor/arc.hpp"
#include "rotor/asio/supervisor_config.h"
#include <boost/asio.hpp>

namespace rotor {
namespace asio {

namespace asio = boost::asio;

struct supervisor_asio_t;
using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

struct system_context_base_t : public arc_base_t<system_context_base_t> {
public:
  system_context_base_t(asio::io_context &io_context_)
      : io_context{io_context_} {}

  /*
  template <typename Supervisor = supervisor_t, typename... Args>
  auto create_supervisor(const supervisor_config_t &config, Args... args)
      -> intrusive_ptr_t<Supervisor>;
  */

  system_context_base_t(const system_context_t &) = delete;
  system_context_base_t(system_context_t &&) = delete;

private:
  friend struct supervisor_asio_t;

  supervisor_ptr_t supervisor;
  asio::io_context &io_context;
};

using system_context_ptr_t = intrusive_ptr_t<system_context_base_t>;

} // namespace asio
} // namespace rotor
