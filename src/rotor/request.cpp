//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/request.h"

using namespace rotor;

void request_subscription_t::set(const void *message_type, const address_ptr_t &source_addr,
                                 const address_ptr_t &dest_addr) noexcept {
    map.emplace(key_t{message_type, source_addr}, dest_addr);
}

address_ptr_t request_subscription_t::get(const void *message_type, const address_ptr_t &source_addr) const noexcept {
    auto it = map.find(key_t{message_type, source_addr});
    if (it == map.end()) {
        return address_ptr_t{};
    }
    return it->second;
}
