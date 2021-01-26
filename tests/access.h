//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#pragma once

#include "rotor.hpp"
#include "rotor/registry.h"

namespace rotor::test {

namespace {
namespace to {
struct identity {};
struct get_plugin {};
struct on_timer_trigger {};
struct state {};
struct shutdown_reason {};
struct internal_infos {};
struct mine_handlers {};
struct actors_map {};
struct points {};
struct locality_leader {};
struct parent_supervisor {};
struct registry {};
struct queue {};
struct own_subscriptions {};
struct request_map {};
struct resources {};
struct last_req_id {};
struct promises {};
struct discovery_map {};
struct forget_link {};
struct tag {};
struct timers_map {};
} // namespace to
} // namespace

bool empty(rotor::subscription_t &subs) noexcept;

} // namespace rotor::test

namespace rotor {

template <> inline auto &actor_base_t::access<test::to::timers_map>() noexcept { return timers_map; }
template <> inline auto &actor_base_t::access<test::to::shutdown_reason>() noexcept { return shutdown_reason; }
template <> inline auto &actor_base_t::access<test::to::state>() noexcept { return state; }
template <> inline auto &actor_base_t::access<test::to::identity>() noexcept { return identity; }
template <> inline auto &actor_base_t::access<test::to::resources>() noexcept { return resources; }

template <> inline auto rotor::actor_base_t::access<test::to::get_plugin, const void *>(const void *identity) noexcept {
    return get_plugin(identity);
}

template <>
inline auto rotor::actor_base_t::access<test::to::on_timer_trigger, request_id_t, bool>(request_id_t request_id,
                                                                                        bool cancelled) noexcept {
    on_timer_trigger(request_id, cancelled);
}

template <> inline auto rotor::subscription_info_t::access<test::to::tag, const void *>(const void *arg) noexcept {
    return tag(arg);
}
template <> inline auto &rotor::subscription_t::access<test::to::internal_infos>() noexcept { return internal_infos; }
template <> inline auto &rotor::subscription_t::access<test::to::mine_handlers>() noexcept { return mine_handlers; }
template <> inline auto &rotor::plugin::plugin_base_t::access<test::to::own_subscriptions>() noexcept {
    return own_subscriptions;
}
template <> inline auto &plugin::child_manager_plugin_t::access<test::to::actors_map>() noexcept { return actors_map; }
template <> inline auto &plugin::lifetime_plugin_t::access<test::to::points>() noexcept { return points; }
template <> inline auto &plugin::resources_plugin_t::access<test::to::resources>() noexcept { return resources; }
template <> inline auto &plugin::registry_plugin_t::access<test::to::discovery_map>() noexcept { return discovery_map; }

template <> inline auto &rotor::supervisor_t::access<test::to::locality_leader>() noexcept { return locality_leader; }
template <> inline auto &rotor::supervisor_t::access<test::to::parent_supervisor>() noexcept { return parent; }
template <> inline auto &rotor::supervisor_t::access<test::to::registry>() noexcept { return registry_address; }
template <> inline auto &rotor::supervisor_t::access<test::to::queue>() noexcept { return queue; }
template <> inline auto &rotor::supervisor_t::access<test::to::request_map>() noexcept { return request_map; }
template <> inline auto &rotor::supervisor_t::access<test::to::last_req_id>() noexcept { return last_req_id; }
template <> inline auto &rotor::registry_t::access<test::to::promises>() noexcept { return promises; }

} // namespace rotor
