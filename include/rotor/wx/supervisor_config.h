#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <chrono>
#include <wx/event.h>

namespace rotor {
namespace wx {

/** \struct supervisor_config_t
 *  \brief wx supervisor config, which holds shutdowm timeout value and
 * a pointer to the "context window".
 */
struct supervisor_config_t {
    /** \brief alias for milliseconds */
    using duration_t = std::chrono::milliseconds;

    /** \brief shutdown timeout value in milliseconds */
    duration_t shutdown_timeout;

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
};

} // namespace wx
} // namespace rotor
