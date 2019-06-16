#pragma once

#include "address.hpp"
#include "error_code.h"
#include <system_error>

namespace rotor {

struct supervisor_t;
using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

struct system_context_t : arc_base_t<system_context_t> {
  public:
    template <typename Supervisor = supervisor_t, typename... Args>
    auto create_supervisor(Args... args) -> intrusive_ptr_t<Supervisor>;
    inline supervisor_ptr_t get_supervisor() noexcept { return supervisor; }

    system_context_t();

    system_context_t(const system_context_t &) = delete;
    system_context_t(system_context_t &&) = delete;
    virtual ~system_context_t();
    virtual void on_error(const std::error_code &ec) noexcept;

  private:
    friend struct supervisor_t;
    supervisor_ptr_t supervisor;
};

using system_context_ptr_t = intrusive_ptr_t<system_context_t>;

} // namespace rotor
