//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/subscription.h"
#include "rotor/supervisor.h"

namespace rotor {

namespace to {
struct subscription_map {};
} // namespace to

template <> auto &supervisor_t::access<to::subscription_map>() noexcept { return subscription_map; }

namespace tags {

const void *io = &io;

}

bool subscription_point_t::operator==(const subscription_point_t &other) const {
    return address == other.address && (*handler == *other.handler);
}

subscription_container_t::iterator subscription_container_t::find(const subscription_point_t &point) noexcept {
    auto predicate = [&point](auto &info) {
        return *info->handler == *point.handler && info->address == point.address;
    };
    auto rit = std::find_if(rbegin(), rend(), predicate);
    if (rit == rend()) {
        return end();
    }
    return --rit.base();
}

void subscription_info_t::tag(const void *t) noexcept {
    auto new_handler = handler->upgrade(t);
    auto &sup = handler->actor_ptr->get_supervisor();
    auto &sm = sup.access<to::subscription_map>();
    sm.update(*this, new_handler);
}

} // namespace rotor
