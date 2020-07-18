//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/subscription.h"
#include "rotor/supervisor.h"
#include "rotor/handler.hpp"

using namespace rotor;

subscription_info_t::~subscription_info_t() {}

subscription_t::subscription_t(supervisor_t &sup_) noexcept : supervisor{sup_} {}

subscription_info_ptr_t subscription_t::materialize(const subscription_point_t &point) noexcept {
    using State = subscription_info_t::state_t;

    auto &address = point.address;
    auto &handler = point.handler;
    bool internal_address = &address->supervisor == &supervisor;
    bool internal_handler = &handler->actor_ptr->get_supervisor() == &supervisor;
    State state = internal_address ? State::SUBSCRIBED : State::SUBSCRIBING;
    subscription_info_ptr_t info(new subscription_info_t(point, internal_address, internal_handler, state));

    if (internal_address) {
        auto &info_list = internal_infos[address];
        info_list.emplace_back(info);

        auto insert_result = mine_handlers.try_emplace({address.get(), handler->message_type});
        auto &joint_handlers = insert_result.first->second;
        auto &handlers = internal_handler ? joint_handlers.internal : joint_handlers.external;
        handlers.emplace_back(handler.get());
    }

    return info;
}

const subscription_t::joint_handlers_t *subscription_t::get_recipients(const message_base_t &message) const noexcept {
    auto address = message.address.get();
    auto message_type = message.type_index;
    auto it = mine_handlers.find({address, message_type});
    if (it == mine_handlers.end()) {
        return nullptr;
    }
    return &it->second;
}

void subscription_t::forget(const subscription_info_ptr_t &info) noexcept {
    if (!info->internal_address)
        return;

    auto &info_container = internal_infos;
    auto infos_it = info_container.find(info->address);
    assert(infos_it != info_container.end());
    auto &info_list = infos_it->second;

    auto info_it = std::find_if(info_list.begin(), info_list.end(),
                                [&info](auto &item) { return item->handler.get() == info->handler.get(); });
    info_list.erase(info_it);
    if (info_list.empty()) {
        info_container.erase(infos_it);
    }

    auto handler_ptr = info->handler.get();
    auto it = mine_handlers.find({info->address.get(), handler_ptr->message_type});
    auto &joint_handlers = it->second;
    auto &handlers = info->internal_handler ? joint_handlers.internal : joint_handlers.external;
    auto &misc_handlers = !info->internal_handler ? joint_handlers.internal : joint_handlers.external;
    auto handler_it =
        std::find_if(handlers.begin(), handlers.end(), [&handler_ptr](auto &item) { return item == handler_ptr; });
    handlers.erase(handler_it);
    if (handlers.empty() && misc_handlers.empty()) {
        mine_handlers.erase(it);
    }
}
