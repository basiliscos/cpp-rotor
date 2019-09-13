//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/wx/supervisor_wx.h"
#include <wx/timer.h>

using namespace rotor::wx;
using namespace rotor;

supervisor_wx_t::timer_t::timer_t(timer_id_t timer_id_, supervisor_ptr_t &&sup_)
    : timer_id{timer_id_}, sup{std::move(sup_)} {}

void supervisor_wx_t::timer_t::Notify() noexcept { sup->on_timer_trigger(timer_id); }

supervisor_wx_t::supervisor_wx_t(supervisor_wx_t *sup, const supervisor_config_wx_t &config_)
    : supervisor_t{sup, config_}, handler{config_.handler} {}

void supervisor_wx_t::start() noexcept {
    supervisor_ptr_t self{this};
    handler->CallAfter([self = std::move(self)]() {
        auto &sup = *self;
        sup.do_process();
    });
}

void supervisor_wx_t::shutdown() noexcept {
    supervisor_ptr_t self{this};
    handler->CallAfter([self = std::move(self)]() {
        auto &sup = *self;
        sup.do_shutdown();
        sup.do_process();
    });
}

void supervisor_wx_t::enqueue(message_ptr_t message) noexcept {
    supervisor_ptr_t self{this};
    handler->CallAfter([self = std::move(self), message = std::move(message)]() {
        auto &sup = *self;
        sup.put(std::move(message));
        sup.do_process();
    });
}

void supervisor_wx_t::start_timer(const rotor::pt::time_duration &timeout, timer_id_t timer_id) noexcept {
    auto self = timer_t::supervisor_ptr_t(this);
    auto timer = std::make_unique<timer_t>(timer_id, std::move(self));
    auto timeout_ms = static_cast<int>(timeout.total_milliseconds());
    timer->StartOnce(timeout_ms);
    timers_map.emplace(timer_id, std::move(timer));
}

void supervisor_wx_t::cancel_timer(timer_id_t timer_id) noexcept {
    auto &timer = timers_map.at(timer_id);
    timer->Stop();
    timers_map.erase(timer_id);
}

void supervisor_wx_t::on_timer_trigger(timer_id_t timer_id) noexcept {
    timers_map.erase(timer_id);
    supervisor_t::on_timer_trigger(timer_id);
    do_process();
}
