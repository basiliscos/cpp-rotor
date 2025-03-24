//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/fltk/supervisor_fltk.h"
#include "rotor/fltk/system_context_fltk.h"

#include <FL/Fl.H>

using namespace rotor;
using namespace rotor::fltk;

namespace {
namespace to {
struct on_timer_trigger {};
struct timers_map {};
} // namespace to
} // namespace

namespace rotor {
template <>
inline auto rotor::actor_base_t::access<to::on_timer_trigger, request_id_t, bool>(request_id_t request_id,
                                                                                  bool cancelled) noexcept {
    on_timer_trigger(request_id, cancelled);
}

template <> inline auto &supervisor_fltk_t::access<to::timers_map>() noexcept { return timers_map; }
} // namespace rotor

static void on_timeout(void *data) noexcept {
    auto timer = reinterpret_cast<supervisor_fltk_t::timer_t *>(data);
    auto *sup = timer->sup.get();

    auto timer_id = timer->handler->request_id;
    auto &timers_map = sup->access<to::timers_map>();

    try {
        auto actor_ptr = timers_map.at(timer_id)->handler->owner;
        actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, false);
        timers_map.erase(timer_id);
        Fl::awake();
        // sup->do_process();
    } catch (std::out_of_range &) {
        // no-op
    }
    intrusive_ptr_release(sup);
}

supervisor_fltk_t::supervisor_fltk_t(supervisor_config_fltk_t &config) : supervisor_t(config) {}

supervisor_fltk_t::timer_t::timer_t(supervisor_ptr_t supervisor_, timer_handler_base_t *handler_)
    : sup(std::move(supervisor_)), handler{handler_} {}

void supervisor_fltk_t::do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept {
    auto self = timer_t::supervisor_ptr_t(this);
    auto timer = std::make_unique<timer_t>(std::move(self), &handler);
    auto seconds = interval.total_nanoseconds() / 1000000000.;
    Fl::add_timeout(seconds, on_timeout, timer.get());
    timers_map.emplace(handler.request_id, std::move(timer));
    intrusive_ptr_add_ref(this);
}

void supervisor_fltk_t::do_cancel_timer(request_id_t timer_id) noexcept {
    try {
        auto &timer = timers_map.at(timer_id);
        Fl::remove_timeout(on_timeout, timer.get());
        auto actor_ptr = timer->handler->owner;
        actor_ptr->access<to::on_timer_trigger, request_id_t, bool>(timer_id, true);
        timers_map.erase(timer_id);
        intrusive_ptr_release(this);
    } catch (std::out_of_range &) {
        // no-op
    }
}

void supervisor_fltk_t::enqueue(message_ptr_t message) noexcept {
    static_cast<system_context_fltk_t*>(context)->enqueue(std::move(message));
}

void supervisor_fltk_t::start() noexcept {
    intrusive_ptr_add_ref(this);
    Fl::awake(
        [](void *data) {
            auto supersisor = reinterpret_cast<supervisor_fltk_t *>(data);
            supersisor->do_process();
            intrusive_ptr_release(supersisor);
        },
        this);
}

void supervisor_fltk_t::shutdown() noexcept {
    intrusive_ptr_add_ref(this);
    Fl::awake(
        [](void *data) {
            auto supersisor = reinterpret_cast<supervisor_fltk_t *>(data);
            supersisor->do_shutdown();
            supersisor->do_process();
            intrusive_ptr_release(supersisor);
        },
        this);
}
