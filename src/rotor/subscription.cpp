//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/subscription.h"
#include "rotor/supervisor.h"
#include "rotor/handler.h"

using namespace rotor;

namespace {
namespace to {
struct internal_address {};
struct internal_handler {};
} // namespace to
} // namespace

template <> auto &subscription_info_t::access<to::internal_address>() noexcept { return internal_address; }
template <> auto &subscription_info_t::access<to::internal_handler>() noexcept { return internal_handler; }

subscription_t::subscription_t(supervisor_t &sup_) noexcept : supervisor{sup_} {}

subscription_info_ptr_t subscription_t::materialize(const subscription_point_t &point) noexcept {
    using State = subscription_info_t::state_t;

    auto &address = point.address;
    auto &handler = point.handler;
    bool internal_address = &address->supervisor == &supervisor;
    bool internal_handler = &handler->actor_ptr->get_supervisor() == &supervisor;
    State state = internal_address ? State::ESTABLISHED : State::SUBSCRIBING;
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

void subscription_t::update(subscription_point_t &point, handler_ptr_t &new_handler) noexcept {
    auto &address = point.address;
    auto &handler = point.handler;
    bool internal_address = &address->supervisor == &supervisor;
    bool internal_handler = &handler->actor_ptr->get_supervisor() == &supervisor;
    if (internal_address) {
        auto it = mine_handlers.find({address.get(), handler->message_type});
        assert(it != mine_handlers.end());
        auto &joint_handlers = it->second;
        auto &handlers = internal_handler ? joint_handlers.internal : joint_handlers.external;
        auto it_handler = std::find(handlers.begin(), handlers.end(), handler.get());
        assert(it_handler != handlers.end());
        *it_handler = new_handler.get();
    }
    point.handler = new_handler;
}

const subscription_t::joint_handlers_t *subscription_t::get_recipients(const message_base_t &message) const noexcept {
    auto address = message.address.get();
    auto message_type = message.type_index;
    auto key = subscrption_key_t{address, message_type};
    try {
        return &mine_handlers.at(key);
    } catch (std::out_of_range &ex) {
        return nullptr;
    }
}

void subscription_t::forget(const subscription_info_ptr_t &info) noexcept {
    if (!info->access<to::internal_address>())
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
    auto internal_handler = info->access<to::internal_handler>();
    auto &handlers = internal_handler ? joint_handlers.internal : joint_handlers.external;
    auto &misc_handlers = !internal_handler ? joint_handlers.internal : joint_handlers.external;
    auto predicate = [&handler_ptr](auto &item) { return item == handler_ptr; };
    auto handler_it = std::find_if(handlers.begin(), handlers.end(), predicate);
    assert(handler_it != handlers.end());
    handlers.erase(handler_it);
    if (handlers.empty() && misc_handlers.empty()) {
        mine_handlers.erase(it);
    }
}
