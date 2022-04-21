#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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
     * Actual rotor-message delivery for actors running on the
     * top of wx-loop is performed via wx-events, i.e. rotor-messages
     * are **wrapped**  into wx-events.
     *
     * The wx-supervisor subscribes to the `wx-events` and unswraps
     * the rotor-messages from the events.
     *
     * The wx event handler is used as a transport medium.
     *
     */
    wxEvtHandler *handler;

    using supervisor_config_t::supervisor_config_t;
};

/** \brief config builder for wx supervisor */
template <typename Supervisor> struct supervisor_config_wx_builder_t : supervisor_config_builder_t<Supervisor> {
    /** \brief final builder class */
    using builder_t = typename Supervisor::template config_builder_t<Supervisor>;

    /** \brief parent config builder */
    using parent_t = supervisor_config_builder_t<Supervisor>;
    using parent_t::parent_t;

    /** \brief bit mask for event handler validation */
    constexpr static const std::uint32_t EVT_LOOP = 1 << 2;

    /** \brief bit mask for all required fields */
    constexpr static const std::uint32_t requirements_mask = parent_t::requirements_mask | EVT_LOOP;

    /** \brief sets event handler */
    builder_t &&handler(wxEvtHandler *handler_) && {
        parent_t::config.handler = handler_;
        parent_t::mask = (parent_t::mask & ~EVT_LOOP);
        return std::move(*static_cast<builder_t *>(this));
    }
};

} // namespace wx
} // namespace rotor
