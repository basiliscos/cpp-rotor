#pragma once

#include "rotor/arc.hpp"
#include "rotor/asio/supervisor_config.h"
#include "rotor/system_context.h"
#include <boost/asio.hpp>

namespace rotor {
namespace asio {

namespace asio = boost::asio;

struct supervisor_asio_t;
using supervisor_ptr_t = intrusive_ptr_t<supervisor_asio_t>;

struct system_context_asio_t : public system_context_t {
    using ptr_t = rotor::intrusive_ptr_t<system_context_asio_t>;

    system_context_asio_t(asio::io_context &io_context_) : io_context{io_context_} {}

    template <typename Supervisor = supervisor_t, typename... Args>
    auto create_supervisor(const supervisor_config_t &config, Args... args) -> intrusive_ptr_t<Supervisor> {
        if (supervisor) {
            on_error(make_error_code(error_code_t::supervisor_defined));
            return intrusive_ptr_t<Supervisor>{};
        } else {
            ptr_t self{this};
            auto typed_sup = system_context_t::create_supervisor<Supervisor>(nullptr, std::move(self), config,
                                                                             std::forward<Args>(args)...);
            supervisor = typed_sup;
            return typed_sup;
        }
    }

    inline asio::io_context &get_io_context() noexcept { return io_context; }

  private:
    friend struct supervisor_asio_t;

    supervisor_ptr_t supervisor;
    asio::io_context &io_context;
};

using system_context_ptr_t = typename system_context_asio_t::ptr_t;

} // namespace asio
} // namespace rotor