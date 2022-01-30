//
// Copyright (c) 2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/spawner.h"
#include "rotor/supervisor.h"
#include <cassert>

using namespace rotor;

namespace {
namespace to {
struct manager {};
} // namespace to
} // namespace

template <> auto &supervisor_t::access<to::manager>() noexcept { return manager; }

spawner_t::spawner_t(factory_t factory_, supervisor_t &supervisor_) noexcept
    : factory{std::move(factory_)}, supervisor{supervisor_} {}

spawner_t::~spawner_t() { spawn(); }

spawner_t &&spawner_t::restart_period(const pt::time_duration &period_) noexcept {
    period = period_;
    return std::move(*this);
}

spawner_t &&spawner_t::restart_policy(restart_policy_t policy_) noexcept {
    policy = policy_;
    return std::move(*this);
}

spawner_t &&spawner_t::max_attempts(size_t attempts_) noexcept {
    attempts = attempts_;
    return std::move(*this);
}

spawner_t &&spawner_t::escalate_failure(bool value) noexcept {
    escalate = value;
    return std::move(*this);
}

void spawner_t::spawn() noexcept {
    if (!done) {
        done = true;
        auto manager = supervisor.access<to::manager>();
        manager->spawn(std::move(factory), period, policy, attempts, escalate);
    }
}
