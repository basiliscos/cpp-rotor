#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor_config.h"
#include <boost/asio.hpp>
#include <memory>

namespace rotor {
namespace asio {

/** \struct supervisor_config_asio_t
 *  \brief boost::asio supervisor config, which holds shutdowm timeout value */
struct supervisor_config_asio_t : public supervisor_config_t {
    /** \brief alias for boost::asio strand type */
    using strand_t = boost::asio::io_context::strand;

    /** \brief type for strand shared pointer */
    using strand_ptr_t = std::shared_ptr<strand_t>;

    /** \brief boost::asio execution strand (shared pointer) */
    strand_ptr_t strand;

    supervisor_config_asio_t(const rotor::pt::time_duration &shutdown_duration, strand_ptr_t strand_)
        : supervisor_config_t{shutdown_duration}, strand{std::move(strand_)} {}
};

} // namespace asio
} // namespace rotor
