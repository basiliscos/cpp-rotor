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

struct ROTOR_FLTK_API supervisor_fltk_t : public supervisor_t {
    using config_t = supervisor_config_fltk_t;

    /** \brief injects templated supervisor_config_wx_builder_t */
    template <typename Supervisor> using config_builder_t = supervisor_config_fltk_builder_t<Supervisor>;

    /** \brief constructs new supervisor from supervisor config */
    supervisor_fltk_t(supervisor_config_fltk_t &config);

    void start() noexcept override;
    void shutdown() noexcept override;
    void enqueue(message_ptr_t message) noexcept override;

    template <typename T> auto &access() noexcept;

    struct timer_t {
        using supervisor_ptr_t = intrusive_ptr_t<supervisor_fltk_t>;

        timer_t(supervisor_ptr_t supervisor, timer_handler_base_t *handler);
        timer_t(timer_t &) = delete;
        timer_t(timer_t &&) = default;

        supervisor_ptr_t sup;
        timer_handler_base_t *handler;
    };

  protected:
    using timer_ptr_t = std::unique_ptr<timer_t>;
    using timers_map_t = std::unordered_map<request_id_t, timer_ptr_t>;

    void do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept override;
    void do_cancel_timer(request_id_t timer_id) noexcept override;

    timers_map_t timers_map;
};


} // namespace fltk
} // namespace rotor
