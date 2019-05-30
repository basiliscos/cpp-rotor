#pragma once

#include "rotor/arc.hpp"
#include "rotor/asio/supervisor_config.h"
#include "rotor/system_context.h"
#include <boost/asio.hpp>
#include <cassert>

namespace rotor {
namespace asio {

namespace asio = boost::asio;

struct supervisor_asio_t;
using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

// struct system_context_asio_t : public arc_base_t<system_context_asio_t> {
struct system_context_asio_t : public system_context_t, arc_base_t<system_context_asio_t> {
    using ptr_t = rotor::intrusive_ptr_t<system_context_asio_t>;

    system_context_asio_t(asio::io_context &io_context_) : io_context{io_context_} {}

    template <typename Supervisor = supervisor_t, typename... Args>
    auto create_supervisor(const supervisor_config_t &config, Args... args) -> intrusive_ptr_t<Supervisor> {
        assert(!supervisor && "supervisor is already defined");
        ptr_t self{this};
        return system_context_t::create_supervisor<Supervisor>(std::move(self), config, std::forward<Args>(args)...);
    }

    system_context_asio_t(const system_context_t &) = delete;
    system_context_asio_t(system_context_t &&) = delete;

  private:
    friend struct supervisor_asio_t;

    supervisor_ptr_t supervisor;
    asio::io_context &io_context;
};

using system_context_ptr_t = typename system_context_asio_t::ptr_t;

} // namespace asio
} // namespace rotor
