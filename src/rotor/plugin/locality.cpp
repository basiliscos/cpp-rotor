//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/locality.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

bool locality_plugin_t::activate(actor_base_t* actor_) noexcept {
    auto& sup =static_cast<supervisor_t&>(*actor_);
    auto& address = sup.address;
    auto parent = sup.parent;
    bool use_other = parent && parent->address->same_locality(*address);
    auto locality_leader = use_other ? parent->locality_leader : &sup;
    sup.locality_leader = locality_leader;
    return plugin_t::activate(actor_);
}

