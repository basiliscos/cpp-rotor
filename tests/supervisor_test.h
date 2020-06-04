#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include <list>

namespace rotor {
namespace test {

extern pt::time_duration default_timeout;

struct supervisor_config_test_t : public supervisor_config_t {
    const void *locality = nullptr;
};

template <typename Supervisor> struct supervisor_test_config_builder_t;

struct supervisor_test_t : public supervisor_t {
    using timers_t = std::list<timer_id_t>;
    using subscription_points_t = internal::lifetime_plugin_t::points_container_t;

    using config_t = supervisor_config_test_t;
    template <typename Supervisor> using config_builder_t = supervisor_test_config_builder_t<Supervisor>;

    supervisor_test_t(supervisor_config_test_t &config_);

    virtual void start_timer(const pt::time_duration &send, timer_id_t timer_id) noexcept override;
    virtual void cancel_timer(timer_id_t timer_id) noexcept override;
    timer_id_t get_timer(std::size_t index) noexcept;
    //virtual void start() noexcept override;
    //virtual void shutdown() noexcept override;
    virtual void enqueue(rotor::message_ptr_t message) noexcept override;
    virtual address_ptr_t make_address() noexcept override;

    state_t &get_state() noexcept { return state; }
    queue_t &get_leader_queue() { return get_leader().queue; }
    supervisor_test_t &get_leader() { return *static_cast<supervisor_test_t *>(locality_leader); }
    subscription_points_t &get_points() noexcept;
    subscription_map_t &get_subscription() noexcept { return subscription_map; }
    auto &get_children() noexcept { return manager->actors_map; }
    request_map_t &get_requests() noexcept { return request_map; }

    plugin_t* get_plugin(const void* identity) const noexcept;
    auto get_activating_plugins() noexcept { return this->activating_plugins; }
    auto get_deactivating_plugins() noexcept { return this->deactivating_plugins; }

    const void *locality;
    timers_t active_timers;
};

template <typename Supervisor> struct supervisor_test_config_builder_t : supervisor_config_builder_t<Supervisor> {
    using parent_t = supervisor_config_builder_t<Supervisor>;
    using parent_t::parent_t;

    supervisor_test_config_builder_t &&locality(const void *locality_) && {
        parent_t::config.locality = locality_;
        return std::move(*this);
    }
};

} // namespace test
} // namespace rotor
