#pragma once

#include "rotor/supervisor.h"
#include "rotor/wx/supervisor_config.h"
#include "rotor/wx/system_context_wx.h"
#include <wx/event.h>
#include <wx/timer.h>

namespace rotor {
namespace wx {

struct shutdown_timer_t : public wxTimer {
    supervisor_wx_t &sup;
    shutdown_timer_t(supervisor_wx_t &sup_);
    virtual void Notify() noexcept override;
};

struct supervisor_wx_t : public supervisor_t {

    supervisor_wx_t(supervisor_wx_t *sup, system_context_ptr_t system_context_, const supervisor_config_t &config);

    virtual void start() noexcept override;
    virtual void shutdown() noexcept override;
    virtual void enqueue(message_ptr_t message) noexcept override;
    virtual void start_shutdown_timer() noexcept override;
    virtual void cancel_shutdown_timer() noexcept override;

    inline system_context_wx_t &get_context() noexcept { return static_cast<system_context_wx_t &>(*context); }

  private:
    supervisor_config_t config;
    shutdown_timer_t shutdown_timer;
};

} // namespace wx
} // namespace rotor
