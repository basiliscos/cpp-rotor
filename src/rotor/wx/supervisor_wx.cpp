//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/wx/supervisor_wx.h"
#include <wx/timer.h>

using namespace rotor::wx;
using namespace rotor;

shutdown_timer_t::shutdown_timer_t(supervisor_wx_t &sup_) : sup{sup_} {}

void shutdown_timer_t::Notify() noexcept { sup.on_shutdown_timer_trigger(); }

supervisor_wx_t::supervisor_wx_t(supervisor_wx_t *sup, const supervisor_config_t &config_)
    : supervisor_t{sup}, config{config_}, shutdown_timer(*this) {}

void supervisor_wx_t::start() noexcept {
    supervisor_ptr_t self{this};
    config.handler->CallAfter([self = std::move(self)]() {
        auto &sup = *self;
        sup.do_process();
    });
}

void supervisor_wx_t::shutdown() noexcept {
    supervisor_ptr_t self{this};
    config.handler->CallAfter([self = std::move(self)]() {
        auto &sup = *self;
        sup.do_shutdown();
        sup.do_process();
    });
}

void supervisor_wx_t::enqueue(message_ptr_t message) noexcept {
    supervisor_ptr_t self{this};
    config.handler->CallAfter([self = std::move(self), message = std::move(message)]() {
        auto &sup = *self;
        sup.put(std::move(message));
        sup.do_process();
    });
}

void supervisor_wx_t::start_shutdown_timer() noexcept {
    auto timeout = static_cast<int>(config.shutdown_timeout.count());
    shutdown_timer.StartOnce(timeout);
}

void supervisor_wx_t::cancel_shutdown_timer() noexcept { shutdown_timer.Stop(); }
