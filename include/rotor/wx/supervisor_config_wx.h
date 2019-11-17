#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor_config.h"
#include <wx/event.h>

namespace rotor {
namespace wx {

/** \struct supervisor_config_wx_t
 *  \brief wx supervisor config, which holds a pointer to the "context window".
 */
struct supervisor_config_wx_t : public supervisor_config_t {
    /** \brief the wx context, responsible for messages delivery
     *
     * Actuall rotor-message delivery for actors runnion on the
     * top of wx-loop is performed via wx-events, i.e. rotor-messages
     * are **wrapped**  into wx-events.
     *
     * The wx-supervisor subscribes to the `wx-events` and unswraps
     * the rotor-messages from the events.
     *
     * The wx event handler is used as a transport medium
     *
     */
    wxEvtHandler *handler;

    /* \brief construct from shutdown timeout and non-owning wx event handler */
    supervisor_config_wx_t(supervisor_t *parent_, pt::time_duration init_timeout_, pt::time_duration shutdown_timeout_,
                           wxEvtHandler *handler_, supervisor_policy_t policy_ = supervisor_policy_t::shutdown_self)
        : supervisor_config_t{parent_, init_timeout_, shutdown_timeout_, policy_}, handler{handler_} {}
};

} // namespace wx
} // namespace rotor
