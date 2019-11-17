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
 *  \brief boost::asio supervisor config, which holds pointer to strand */
struct supervisor_config_asio_t : public supervisor_config_t {
    /** \brief alias for boost::asio strand type */
    using strand_t = boost::asio::io_context::strand;

    /** \brief type for strand shared pointer */
    using strand_ptr_t = std::shared_ptr<strand_t>;

    /** \brief boost::asio execution strand (shared pointer) */
    strand_ptr_t strand;

    /* \brief constructs config from shutdown timeout and shared pointer to strand  */
    supervisor_config_asio_t(supervisor_t *parent_, pt::time_duration init_timeout_,
                             pt::time_duration shutdown_timeout_, strand_ptr_t strand_,
                             supervisor_policy_t policy_ = supervisor_policy_t::shutdown_self)
        : supervisor_config_t{parent_, init_timeout_, shutdown_timeout_, policy_}, strand{std::move(strand_)} {}
};

} // namespace asio
} // namespace rotor
