#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "actor_test.h"
#include <list>

namespace rotor {
namespace test {

extern pt::time_duration default_timeout;

using interceptor_t = std::function<void(message_ptr_t &, const void *, const continuation_t &)>;

struct supervisor_config_test_t : public supervisor_config_t {
    const void *locality = nullptr;
    plugin_configurer_t configurer = plugin_configurer_t{};
    interceptor_t interceptor = interceptor_t{};
};

template <typename Supervisor> struct supervisor_test_config_builder_t;

struct supervisor_test_t : public supervisor_t {
    using timers_t = std::list<timer_handler_base_t *>;

    using config_t = supervisor_config_test_t;
    template <typename Supervisor> using config_builder_t = supervisor_test_config_builder_t<Supervisor>;

    supervisor_test_t(supervisor_config_test_t &config_);
    ~supervisor_test_t();

    void configure(plugin::plugin_base_t &plugin) noexcept override;
    virtual void do_start_timer(const pt::time_duration &interval, timer_handler_base_t &handler) noexcept override;
    virtual void do_cancel_timer(request_id_t timer_id) noexcept override;
    void do_invoke_timer(request_id_t timer_id) noexcept;
    request_id_t get_timer(std::size_t index) noexcept;
    virtual void start() noexcept override {}
    virtual void shutdown() noexcept override {}
    virtual void enqueue(rotor::message_ptr_t message) noexcept override;
    virtual address_ptr_t make_address() noexcept override;
    void intercept(message_ptr_t &message, const void *tag, const continuation_t &continuation) noexcept override;

    state_t &get_state() noexcept { return state; }
    messages_queue_t &get_leader_queue() { return get_leader().queue; }
    supervisor_test_t &get_leader();
    subscription_container_t &get_points() noexcept;
    subscription_t &get_subscription() noexcept { return subscription_map; }
    size_t get_children_count() noexcept;
    request_map_t &get_requests() noexcept { return request_map; }

    auto get_activating_plugins() noexcept { return this->activating_plugins; }
    auto get_deactivating_plugins() noexcept { return this->deactivating_plugins; }

    template <typename Plugin> Plugin *get_casted_plugin() noexcept {
        for (auto &p : plugins) {
            if (p->identity() == Plugin::class_identity) {
                return static_cast<Plugin *>(p);
            }
        }
        return nullptr;
    }

    const void *locality;
    timers_t active_timers;
    plugin_configurer_t configurer;
    interceptor_t interceptor;
};
using supervisor_test_ptr_t = rotor::intrusive_ptr_t<supervisor_test_t>;

template <typename Supervisor> struct supervisor_test_config_builder_t : supervisor_config_builder_t<Supervisor> {
    using builder_t = typename Supervisor::template config_builder_t<Supervisor>;
    using parent_t = supervisor_config_builder_t<Supervisor>;
    using parent_t::parent_t;

    builder_t &&locality(const void *locality_) && {
        parent_t::config.locality = locality_;
        return std::move(*this);
    }

    builder_t &&configurer(plugin_configurer_t &&value) && {
        parent_t::config.configurer = std::move(value);
        return std::move(*this);
    }

    builder_t &&interceptor(interceptor_t &&value) && {
        parent_t::config.interceptor = std::move(value);
        return std::move(*this);
    }
};

struct system_test_context_t : system_context_t {
    using system_context_t::system_context_t;
    ~system_test_context_t();
};

} // namespace test
} // namespace rotor
