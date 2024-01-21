//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <iostream>
#include <sstream>
#include <ios>

#include "rotor/supervisor.h"
#include "rotor/system_context.h"
#include "rotor/misc/default_stringifier.h"

namespace rotor {

namespace {
namespace to {
struct inbound_queue {};
} // namespace to
} // namespace

template <> auto &supervisor_t::access<to::inbound_queue>() noexcept { return inbound_queue; }
} // namespace rotor

using namespace rotor;

void system_context_t::on_error(actor_base_t *actor, const extended_error_ptr_t &ec) noexcept {
    std::cerr << "fatal error ";
    if (actor) {
        std::cerr << actor->get_identity();
    }
    std::cerr << ": " << ec->message() << "\n";
    std::terminate();
}

std::string system_context_t::identity() noexcept {
    std::stringstream out;
    out << std::hex << (const void *)this;
    return out.str();
}

system_context_t::~system_context_t() {
    if (supervisor) {
        message_base_t *ptr;
        while (supervisor->access<to::inbound_queue>().pop(ptr)) {
            intrusive_ptr_release(ptr);
        }
    }
}

auto system_context_t::get_stringifier() -> const message_stringifier_t & {
    if (!stringifier) {
        stringifier = make_stringifier();
    }
    return *stringifier;
}

auto system_context_t::make_stringifier() const noexcept -> message_stringifier_ptr_t {
    return message_stringifier_ptr_t(new misc::default_stringifier_t());
}
