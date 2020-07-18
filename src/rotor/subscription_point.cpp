//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/subscription.h"
#include "rotor/handler.hpp"

using namespace rotor;

bool subscription_point_t::operator==(const subscription_point_t &other) const noexcept {
    return address == other.address && (*handler == *other.handler);
}

subscription_container_t::iterator subscription_container_t::find(const subscription_point_t &point) noexcept {
    auto predicate = [&point](auto &info) {
        return *info->handler == *point.handler && info->address == point.address;
    };
    auto rit = std::find_if(rbegin(), rend(), predicate);
    return --rit.base();
}
