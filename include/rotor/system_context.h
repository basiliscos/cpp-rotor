#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "error_code.h"
#include <system_error>

namespace rotor {

struct supervisor_t;
using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

/** \struct system_context_t
 *  \brief The system context holds root {@link supervisor_t}
 * (intrusive pointer) and may be loop-related details in derived classes
 *
 */
struct system_context_t : arc_base_t<system_context_t> {
  public:
    /** \brief creates root supervior. `args` are forwared for supervisor constructor */
    template <typename Supervisor = supervisor_t, typename... Args>
    auto create_supervisor(Args... args) -> intrusive_ptr_t<Supervisor>;

    /** \brief returns root supervisor */
    inline supervisor_ptr_t get_supervisor() noexcept { return supervisor; }

    system_context_t();

    system_context_t(const system_context_t &) = delete;
    system_context_t(system_context_t &&) = delete;
    virtual ~system_context_t();

    /** \brief fatal error handler
     *
     * The error is fatal, is further `rotor` behaviour is undefined. The method should
     * be overriden in derived classes for error propagation/notification. The default
     * implementation is to output the error to `std::err` and invoke `std::abort()`.
     *
     */
    virtual void on_error(const std::error_code &ec) noexcept;

  private:
    friend struct supervisor_t;
    supervisor_ptr_t supervisor;
};

/** \brief intrusive pointer for system context */
using system_context_ptr_t = intrusive_ptr_t<system_context_t>;

} // namespace rotor
