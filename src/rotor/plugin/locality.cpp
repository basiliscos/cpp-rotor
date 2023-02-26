//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/locality.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct parent {};
struct locality_leader {};
struct inbound_queue {};
struct inbound_queue_size {};
} // namespace to
} // namespace

template <> auto &supervisor_t::access<to::parent>() noexcept { return parent; }
template <> auto &supervisor_t::access<to::locality_leader>() noexcept { return locality_leader; }
template <> auto &supervisor_t::access<to::inbound_queue>() noexcept { return inbound_queue; }
template <> auto &supervisor_t::access<to::inbound_queue_size>() noexcept { return inbound_queue_size; }

const std::type_index locality_plugin_t::class_identity = typeid(locality_plugin_t);

const std::type_index &locality_plugin_t::identity() const noexcept { return class_identity; }

void locality_plugin_t::activate(actor_base_t *actor_) noexcept {
    auto &address = actor_->get_address();
    auto &sup = static_cast<supervisor_t &>(*actor_);
    auto &parent = sup.access<to::parent>();
    bool use_other = parent && static_cast<actor_base_t *>(parent)->get_address()->same_locality(*address);
    auto locality_leader = use_other ? parent->access<to::locality_leader>() : &sup;
    sup.access<to::locality_leader>() = locality_leader;
    if (!use_other) {
        auto sz = sup.access<to::inbound_queue_size>();
        sup.access<to::inbound_queue>().reserve_unsafe(sz);
    }
    return plugin_base_t::activate(actor_);
}
