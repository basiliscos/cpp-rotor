#pragma once
#include <boost/asio.hpp>

#include "address.hpp"
#include "supervisor.hpp"

namespace rotor {

namespace asio = boost::asio;

struct system_context_t {
public:
  // system_context_t(asio::io_context &io_context_): io_context{io_context_} {}

  template <typename Supervisor = supervisor_t, typename... Args>
  system_context_t(asio::io_context &io_context_, Args... args)
      : io_context{io_context_} {
    auto raw_object = new Supervisor{*this, std::forward<Args>(args)...};
    supervisor = supervisor_ptr_t{raw_object};
  }

  system_context_t(const system_context_t &) = delete;
  system_context_t(system_context_t &&) = delete;

protected:
  address_ptr_t make_address() {
    return address_ptr_t{new address_t(static_cast<void *>(this))};
  }

private:
  supervisor_ptr_t supervisor;
  asio::io_context &io_context;
};

} // namespace rotor
