#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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

    bool guard_context = false;

    using supervisor_config_t::supervisor_config_t;
};

/** \brief CRTP supervisor asio config builder */
template <typename Supervisor> struct supervisor_config_asio_builder_t : supervisor_config_builder_t<Supervisor> {
    using builder_t = typename Supervisor::template config_builder_t<Supervisor>;
    using parent_t = supervisor_config_builder_t<Supervisor>;
    using parent_t::parent_t;
    using strand_ptr_t = supervisor_config_asio_t::strand_ptr_t;

    constexpr static const std::uint32_t STRAND = 1 << 2;
    constexpr static const std::uint32_t requirements_mask = parent_t::requirements_mask | STRAND;

    builder_t &&strand(strand_ptr_t &strand) && {
        parent_t::config.strand = strand;
        parent_t::mask = (parent_t::mask & ~STRAND);
        return std::move(*static_cast<builder_t *>(this));
    }

    builder_t &&guard_context(bool value) && {
        parent_t::config.guard_context = value;
        return std::move(*static_cast<builder_t *>(this));
    }
};

} // namespace asio
} // namespace rotor
