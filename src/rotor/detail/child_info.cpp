//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/detail/child_info.h"

using namespace rotor::detail;

void child_info_t::spawn_attempt() noexcept {
    initialized = started = false;
    ++attempts;
    timer_id = 0;
    last_instantiation = pt::microsec_clock::local_time();
    if (max_attempts && attempts >= max_attempts) {
        active = false;
    }
}
