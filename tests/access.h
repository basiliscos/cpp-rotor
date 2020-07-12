//
// Copyright (c) 2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//


#pragma once

#include "rotor.hpp"

namespace rotor::test {

namespace {
namespace to {
struct address {};
struct get_plugin {};
struct state {};
struct supervisor {};
struct internal_infos {};
struct mine_handlers {};
struct actors_map {};
struct points {};
struct locality_leader {};
struct parent_supervisor {};
struct registry {};
} // namespace to
} // namespace


bool empty(rotor::subscription_t &subs) noexcept;

}

namespace rotor {

template <> inline auto &actor_base_t::access<test::to::address>() noexcept { return address; }
template <> inline auto &actor_base_t::access<test::to::state>() noexcept { return state; }
template <> inline auto &actor_base_t::access<test::to::supervisor>() noexcept { return supervisor; }

template <> inline auto rotor::actor_base_t::access<test::to::get_plugin, const void *>(const void *identity) noexcept {
    return get_plugin(identity);
}


template <> auto &rotor::subscription_t::access<test::to::internal_infos>() noexcept { return internal_infos; }
template <> auto &rotor::subscription_t::access<test::to::mine_handlers>() noexcept { return mine_handlers; }
template <> auto &internal::child_manager_plugin_t::access<test::to::actors_map>() noexcept { return actors_map; }
template <> auto &internal::lifetime_plugin_t::access<test::to::points>() noexcept { return points; }
template <> auto &rotor::supervisor_t::access<test::to::locality_leader>() noexcept { return locality_leader; }
template <> auto &rotor::supervisor_t::access<test::to::parent_supervisor>() noexcept { return parent; }
template <> auto &rotor::supervisor_t::access<test::to::registry>() noexcept { return registry_address; }

}
