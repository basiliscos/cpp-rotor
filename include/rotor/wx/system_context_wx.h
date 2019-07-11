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
using supervisor_ptr_t = intrusive_ptr_t<supervisor_wx_t>;

struct system_context_wx_t : public system_context_t {
    using ptr_t = rotor::intrusive_ptr_t<system_context_wx_t>;

    system_context_wx_t(wxAppConsole *app = nullptr);

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

    inline wxAppConsole *get_app() noexcept { return app; }

  private:
    friend struct supervisor_wx_t;

    supervisor_ptr_t supervisor;
    wxAppConsole *app;
};

using system_context_ptr_t = typename system_context_wx_t::ptr_t;

} // namespace wx
} // namespace rotor
