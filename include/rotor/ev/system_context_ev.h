#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/arc.hpp"
#include "rotor/ev/supervisor_config.h"
#include "rotor/system_context.h"
#include <ev.h>

namespace rotor {
namespace ev {

struct supervisor_ev_t;

/** \brief intrusive pointer for ev supervisor */
using supervisor_ptr_t = intrusive_ptr_t<supervisor_ev_t>;

/** \struct system_context_ev_t
 *
 */
struct system_context_ev_t : public system_context_t {
    /** \brief intrusive pointer type for ev system context */
    using ptr_t = rotor::intrusive_ptr_t<system_context_ev_t>;

    system_context_ev_t();

    /** \brief creates root supervior. `args` and config are forwared for supervisor constructor */
    template <typename Supervisor = supervisor_t, typename... Args>
    auto create_supervisor(const supervisor_config_t &config, Args... args) -> intrusive_ptr_t<Supervisor> {
        if (supervisor) {
            on_error(make_error_code(error_code_t::supervisor_defined));
            return intrusive_ptr_t<Supervisor>{};
        } else {
            auto typed_sup =
                system_context_t::create_supervisor<Supervisor>(nullptr, config, std::forward<Args>(args)...);
            supervisor = typed_sup;
            return typed_sup;
        }
    }

  protected:
    friend struct supervisor_ev_t;

    /** \brief root ev supervisor */
    supervisor_ptr_t supervisor;
};

/** \brief intrusive pointer type for ev system context */
using system_context_ptr_t = typename system_context_ev_t::ptr_t;

} // namespace ev
} // namespace rotor
