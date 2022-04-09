#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/arc.hpp"
#include "rotor/system_context.h"
#include <boost/asio.hpp>

namespace rotor {
namespace asio {

namespace asio = boost::asio;

struct ROTOR_API supervisor_asio_t;

/** \brief intrusive pointer for boost::asio supervisor */
using supervisor_ptr_t = intrusive_ptr_t<supervisor_asio_t>;

/** \struct system_context_asio_t
 *  \brief The boost::asio system context, which holds a reference to
 * `boost::asio::io_context` and root supervisor
 */
struct ROTOR_API system_context_asio_t : public system_context_t {
    /** \brief intrusive pointer type for boost::asio system context */
    using ptr_t = rotor::intrusive_ptr_t<system_context_asio_t>;

    /** \brief construct the context from `boost::asio::io_context` reference */
    system_context_asio_t(asio::io_context &io_context_) : io_context{io_context_} {}

    /** \brief returns a reference to `boost::asio::io_context` */
    inline asio::io_context &get_io_context() noexcept { return io_context; }

  protected:
    friend struct supervisor_asio_t;

    /** \brief a reference to `boost::asio::io_context` */
    asio::io_context &io_context;
};

/** \brief intrusive pointer type for boost::asio system context */
using system_context_ptr_t = typename system_context_asio_t::ptr_t;

} // namespace asio
} // namespace rotor
