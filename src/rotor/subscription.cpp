//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/subscription.h"
#include "rotor/supervisor.h"
#include "rotor/handler.hpp"

using namespace rotor;

subscription_t::subscription_t(supervisor_t &sup) : supervisor{sup} {}
subscription_t::~subscription_t() {}

void subscription_t::subscribe(handler_ptr_t handler) {
    bool mine = &handler->actor_ptr->get_supervisor() == &supervisor;
    map[handler->message_type].emplace_back(classified_handlers_t{std::move(handler), mine});
}

subscription_t::list_t *subscription_t::get_recipients(const subscription_t::message_type_t &slot) noexcept {
    auto it = map.find(slot);
    if (it != map.end()) {
        return &it->second;
    }
    return nullptr;
}

std::size_t subscription_t::unsubscribe(handler_ptr_t handler) {
    auto &list = map.at(handler->message_type);
    auto it = list.begin();
    while (it != list.end()) {
        if (*it->handler == *handler) {
            it = list.erase(it);
        } else {
            ++it;
        }
    }
    if (list.empty()) {
        map.erase(handler->message_type);
    }
    return map.size();
}

