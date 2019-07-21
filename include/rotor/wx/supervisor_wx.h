#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "rotor/wx/supervisor_config.h"
#include "rotor/wx/system_context_wx.h"
#include <wx/event.h>
#include <wx/timer.h>

namespace rotor {
namespace wx {

/** \struct shutdown_timer_t
 *  \brief timer structure, adoped for wx-supervisor needs.
 *
 */
struct shutdown_timer_t : public wxTimer {
    /** \brief pointer to wx supervisor */
    supervisor_wx_t &sup;

    /** \brief constructs timer from ex supervisor */
    shutdown_timer_t(supervisor_wx_t &sup_);

    /** \brief invokes `shutdown_timer_trigger` method if shutdown timer triggers*/
    virtual void Notify() noexcept override;
};

/** \struct supervisor_wx_t
 *  \brief delivers rotor-messages on top of wx event
 *
 *  Basically wx event handler is used as transport for wx-rotor-events,
 *  which wrap rotor-messages.
 *
 * Since wx-event loop is mainly GUI-loop, i.e. singleton, there are no
 * different execution contexts, i.e. only one (main) executing thread;
 * hence, there is no sense of having multiple supervisors.
 *
 * The wx-supervisor (and it's actors) role in `rotor` messaging is
 * to **abstract** destination endpoints from other non-wx (I/O) event
 * loops, i.e. show some information in corresponding field/window/frame/...
 * Hence, the receiving rotor-messages actors should be aware of your
 * wx-application system, i.e. hold refences to appropriate fields/windows
 * etc.
 *
 * The commands transalaton (i.e. from click on the button to the `rotor`
 * -message send) should be also performed on wx-specific actors.
 *
 */
struct supervisor_wx_t : public supervisor_t {

    /** \brief constructs new supervisor from parent supervisor and supervisor config
     *
     * the `parent` supervisor can be null
     *
     */
    supervisor_wx_t(supervisor_wx_t *parent, const supervisor_config_t &config);

    /** \brief creates an actor by forwaring `args` to it
     *
     * The newly created actor belogs to the wx supervisor / wx event loop
     */
    template <typename Actor, typename... Args> intrusive_ptr_t<Actor> create_actor(Args... args) {
        return make_actor<Actor>(*this, std::forward<Args>(args)...);
    }

    virtual void start() noexcept override;
    virtual void shutdown() noexcept override;
    virtual void enqueue(message_ptr_t message) noexcept override;
    virtual void start_shutdown_timer() noexcept override;
    virtual void cancel_shutdown_timer() noexcept override;

    /** \brief returns pointer to the wx system context */
    inline system_context_wx_t *get_context() noexcept { return static_cast<system_context_wx_t *>(context); }

  protected:
    /** \brief timeout value and wx events transport */
    supervisor_config_t config;

    /** \brief timer used in shutdown procedure */
    shutdown_timer_t shutdown_timer;
};

} // namespace wx
} // namespace rotor
