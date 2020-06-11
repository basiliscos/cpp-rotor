//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/subscription.h"
#include "rotor/supervisor.h"
#include "rotor/handler.hpp"

using namespace rotor;

subscription_info_t::subscription_info_t(const handler_ptr_t& handler_, const address_ptr_t& address_,
                                         bool internal_address_, bool internal_handler_, state_t state_) noexcept:
    handler{handler_}, address{address_}, internal_address{internal_address_}, internal_handler{internal_handler_}, state{state_} {}

subscription_info_t::~subscription_info_t(){};

subscription_t::subscription_t(supervisor_t& sup_)noexcept :supervisor{sup_}{}

subscription_info_ptr_t subscription_t::materialize(const handler_ptr_t& handler, const address_ptr_t& address) noexcept {
    using State = subscription_info_t::state_t;

    bool internal_address = &address->supervisor == &supervisor;
    bool internal_handler = &handler->actor_ptr->get_supervisor() == &supervisor;
    State state = internal_address ? State::SUBSCRIBED : State::SUBSCRIBING;
    subscription_info_ptr_t info(new subscription_info_t(handler, address, internal_address, internal_handler, state));

    if (internal_address) {
        auto& info_list = internal_infos[address];
        info_list.emplace_back(info);
    }

    if (internal_address) {
        auto insert_result = mine_handlers.try_emplace({address.get(), handler->message_type});
        auto& joint_handlers = insert_result.first->second;
        auto& handlers = internal_handler ? joint_handlers.internal : joint_handlers.external;
        handlers.emplace_back(handler.get());
    }

    return info;
}

const subscription_t::joint_handlers_t* subscription_t::get_recipients(const message_base_t& message) const noexcept {
    auto address = message.address.get();
    auto message_type = message.type_index;
    auto it = mine_handlers.find({ address, message_type});
    if (it == mine_handlers.end()) {
        return nullptr;
    }
    return &it->second;
}

void subscription_t::forget(const subscription_info_ptr_t &info) noexcept {
    using State = subscription_info_t::state_t;

    assert(info->internal_address || info->internal_handler);
    auto& info_container = info->internal_address ? internal_infos : external_infos;
    auto infos_it = info_container.find(info->address);
    auto& info_list = infos_it->second;

    auto info_it = std::find_if(info_list.begin(), info_list.end(), [&info](auto& item) { return item->handler.get() == info->handler.get(); } );
    info_list.erase(info_it);
    if (info_list.empty()) {
        info_container.erase(infos_it);
    }

    if (!info->internal_address) return;
    auto handler_ptr = info->handler.get();
    auto it = mine_handlers.find({info->address.get(), handler_ptr->message_type});
    auto& joint_handlers = it->second;
    auto& handlers = info->internal_handler ? joint_handlers.internal : joint_handlers.external;
    auto& misc_handlers = !info->internal_handler ? joint_handlers.internal : joint_handlers.external;
    auto handler_it = std::find_if(handlers.begin(), handlers.end(), [&handler_ptr](auto& item) { return item == handler_ptr; } );
    handlers.erase(handler_it);
    if (handlers.empty() && misc_handlers.empty()) {
        mine_handlers.erase(it);
    }
}


bool subscription_t::empty() const noexcept {
    return internal_infos.empty() && external_infos.empty() && mine_handlers.empty();
}


subscription_container_t::iterator subscription_container_t::find(const subscription_point_t& point) noexcept {
    auto predicate = [&point](auto& info) { return info->handler == point.handler && info->address == point.address; };
    auto rit = std::find_if(rbegin(), rend(), predicate);
    assert(rit != rend());
    return --rit.base();
}
