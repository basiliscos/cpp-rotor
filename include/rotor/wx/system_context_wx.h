#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/arc.hpp"
#include "rotor/wx/supervisor_config.h"
#include "rotor/system_context.h"
#include <wx/app.h>

namespace rotor {
namespace wx {

struct supervisor_wx_t;

/** \brief intrusive pointer for wx supervisor */
using supervisor_ptr_t = intrusive_ptr_t<supervisor_wx_t>;

/** \struct system_context_wx_t
 *  \brief The wxWidgets system context, which holds a pointer to wxApp and
 * root wx-supervisor
 *
 */
struct system_context_wx_t : public system_context_t {
    /** \brief intrusive pointer type for wx system context */
    using ptr_t = rotor::intrusive_ptr_t<system_context_wx_t>;

    /** \brief construct the context from wx application
     *
     * If `app` is null, then the wx application is taken via `wxTheApp` macro
     *
     */
    system_context_wx_t(wxAppConsole *app = nullptr);

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

    /** \brief returns wx application */
    inline wxAppConsole *get_app() noexcept { return app; }

  protected:
    friend struct supervisor_wx_t;

    /** \brief root wx supervisor */
    supervisor_ptr_t supervisor;

    /** \brief non-owning pointer to the wx application */
    wxAppConsole *app;
};

/** \brief intrusive pointer type for wx system context */
using system_context_ptr_t = typename system_context_wx_t::ptr_t;

} // namespace wx
} // namespace rotor
