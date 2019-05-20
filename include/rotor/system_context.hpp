#pragma once
#include <boost/asio.hpp>

#include "address.hpp"
#include "supervisor.hpp"

namespace rotor {

namespace asio = boost::asio;

struct supervisor_t;
using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

struct system_context_t {
public:
  system_context_t(asio::io_context &io_context_) : io_context{io_context_} {}

  template <typename Supervisor = supervisor_t, typename... Args>
  auto create_supervisor(Args... args) -> intrusive_ptr_t<Supervisor>;

  system_context_t(const system_context_t &) = delete;
  system_context_t(system_context_t &&) = delete;

private:
  friend struct supervisor_t;
  supervisor_ptr_t supervisor;
  asio::io_context &io_context;
};

} // namespace rotor
