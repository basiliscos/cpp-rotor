#pragma once

//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "supervisor_config.h"
#include "extended_error.h"
#include "message_stringifier.h"

#include <system_error>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace rotor {

struct supervisor_t;
struct actor_base_t;
using supervisor_ptr_t = intrusive_ptr_t<supervisor_t>;

/** \struct system_context_t
 *  \brief The system context holds root {@link supervisor_t}
 * (intrusive pointer) and may be loop-related details in derived classes
 *
 */
struct ROTOR_API system_context_t : arc_base_t<system_context_t> {
  public:
    /** \brief returns builder for root supervisor */
    template <typename Supervisor = supervisor_t> auto create_supervisor();

    /** \brief returns root supervisor */
    inline supervisor_ptr_t get_supervisor() noexcept { return supervisor; }

    system_context_t() = default;

    system_context_t(const system_context_t &) = delete;
    system_context_t(system_context_t &&) = delete;
    virtual ~system_context_t();

    /** \brief fatal error handler
     *
     * The error is fatal, is further `rotor` behavior is undefined. The method should
     * be overriden in derived classes for error propagation/notification. The default
     * implementation is to output the error to `std::err` and invoke `std::abort()`.
     *
     */
    virtual void on_error(actor_base_t *actor, const extended_error_ptr_t &ec) noexcept;

    /** \brief identifies the context.
     *
     *  By default, is is just human-readable address of the system context
     *
     */
    virtual std::string identity() noexcept;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

    /** \brief returns the default stringifier
     *
     *  The stringifier is lazyly constructed on demand via `make_stringifier` method.
     *
     */
    const message_stringifier_t &get_stringifier();

  protected:
    /** \brief constructs message stringifier */
    virtual message_stringifier_ptr_t make_stringifier() const noexcept;

  private:
    friend struct supervisor_t;
    supervisor_ptr_t supervisor;
    message_stringifier_ptr_t stringifier;
};

/** \brief intrusive pointer for system context */
using system_context_ptr_t = intrusive_ptr_t<system_context_t>;

} // namespace rotor

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
