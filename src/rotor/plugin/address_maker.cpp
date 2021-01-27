//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/address_maker.h"
#include "rotor/actor_base.h"
#include "rotor/supervisor.h"
#include <typeinfo>
#include <sstream>
#include <string>
#include <string_view>
#include <ostream>
#include <ios>

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct address_maker {};
struct address {};
struct identity {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::address_maker>() noexcept { return address_maker; }
template <> auto &actor_base_t::access<to::address>() noexcept { return address; }
template <> auto &actor_base_t::access<to::identity>() noexcept { return identity; }

const void *address_maker_plugin_t::class_identity = static_cast<const void *>(typeid(address_maker_plugin_t).name());

const void *address_maker_plugin_t::identity() const noexcept { return class_identity; }

void address_maker_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;

    if (!actor->get_address()) {
        actor->access<to::address>() = create_address();
    }
    actor->access<to::address_maker>() = this;
    plugin_base_t::activate(actor_);
    actor_->configure(*this);
    if (actor->access<to::identity>().empty()) {
        generate_identity();
    }
    actor->init_start();
}

void address_maker_plugin_t::deactivate() noexcept {
    actor->access<to::address_maker>() = nullptr;
    return plugin_base_t::deactivate();
}

address_ptr_t address_maker_plugin_t::create_address() noexcept { return actor->get_supervisor().make_address(); }

void address_maker_plugin_t::set_identity(std::string_view name, bool append_addr) noexcept {
    if (!append_addr) {
        actor->access<to::identity>() = name;
        return;
    }
    std::stringstream out;
    auto &addr = actor->access<to::address>();
    out << name << " ";
    out << std::hex << (const void *)addr.get();
    actor->access<to::identity>() = out.str();
}

void address_maker_plugin_t::generate_identity() noexcept {
    set_identity(dynamic_cast<supervisor_t *>(actor) ? "supervisor" : "actor");
}
