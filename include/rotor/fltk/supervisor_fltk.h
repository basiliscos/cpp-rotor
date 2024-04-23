#pragma once

//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "rotor/fltk/export.h"
#include "rotor/fltk/supervisor_config_fltk.h"

#include <memory>
#include <unordered_map>

namespace rotor {
namespace fltk {

/** \struct supervisor_fltk_t
 *  \brief delivers rotor-messages on top of fltk async callback
 *
 * The fltk-supervisor (and it's actors) role in `rotor` messaging is
 * to **abstract** destination endpoints from other non-fltk (I/O) event
 * loops, i.e. show some information in corresponding field/window/frame/...
 * Hence, the receiving rotor-messages actors should be aware of your
 * fltk-application system, i.e. hold refences to appropriate fields/windows
 * /widgets etc.
 *
 * Since conceptually, fltk-event loop is main GUI-loop, i.e. singleton,
 * there are no different execution contexts, i.e. only one (main)
 * executing thread; hence, there is no sense of having multiple supervisors.
 *
 * The commands translator (i.e. from click on the button to the `rotor`
 * -message send) should be also performed fltk-widgets which hold corresponding
 * fltk- actor/supervisor.
 *
 */
struct ROTOR_FLTK_API supervisor_fltk_t : public supervisor_t {
    /** \brief injects an alias for supervisor_config_fltk_t */
    using config_t = supervisor_config_fltk_t;

    /** \brief injects templated supervisor_config_fltk_builder_t */
    template <typename Supervisor> using config_builder_t = supervisor_config_fltk_builder_t<Supervisor>;

    /** \brief constructs new supervisor from supervisor config */
    supervisor_fltk_t(supervisor_config_fltk_t &config);

    void start() noexcept override;
    void shutdown() noexcept override;
    void enqueue(message_ptr_t message) noexcept override;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

    /** \struct timer_t
     *  \brief timer structure, adopted for fltk-supervisor needs.
     */
    struct timer_t {
        /** \brief alias for intrusive pointer for the fltk supervisor */
        using supervisor_ptr_t = intrusive_ptr_t<supervisor_fltk_t>;

        /** \brief constructs timer from flt supervisor and timer handler */
        timer_t(supervisor_ptr_t supervisor, timer_handler_base_t *handler);

        /** \brief intrusive pointer to the supervisor */
        supervisor_ptr_t sup;

        /** \brief non-owning pointer to timer handler */
        timer_handler_base_t *handler;
    };

  protected:
    /** \brief unique pointer to timer */
    using timer_ptr_t = std::unique_ptr<timer_t>;

    /** \brief timer id to timer pointer mapping type */
    using timers_map_t = std::unordered_map<request_id_t, timer_ptr_t>;

    void do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept override;
    void do_cancel_timer(request_id_t timer_id) noexcept override;

    /** \brief timer id to timer pointer mapping */
    timers_map_t timers_map;
};

} // namespace fltk
} // namespace rotor
