//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/delivery.h"
#include "rotor/supervisor.h"
#include "rotor/messages.hpp"
#include <boost/core/demangle.hpp>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <charconv>

using namespace rotor;
using namespace rotor::plugin;

delivery_plugin_base_t::~delivery_plugin_base_t() {}

void delivery_plugin_base_t::activate(actor_base_t *actor_) noexcept {
    plugin_base_t::activate(actor_);
    auto sup = static_cast<supervisor_t *>(actor_);
    queue = &sup->locality_leader->queue;
    address = sup->address.get();
    subscription_map = &sup->subscription_map;
    sup->delivery = this;
}

void local_delivery_t::delivery(message_ptr_t &message,
                                const subscription_t::joint_handlers_t &local_recipients) noexcept {
    for (auto handler : local_recipients.external) {
        auto &sup = handler->actor_ptr->get_supervisor();
        auto &address = sup.get_address();
        auto wrapped_message = make_message<payload::handler_call_t>(address, message, handler);
        sup.enqueue(std::move(wrapped_message));
    }
    for (auto handler : local_recipients.internal) {
        handler->call(message);
    }
}

std::string inspected_local_delivery_t::identify(message_base_t *message, std::int32_t threshold) noexcept {
    using boost::core::demangle;
    using T = owner_tag_t;
    std::string info = demangle((const char *)message->type_index);
    std::int32_t level = 0;
    auto dump_point = [](subscription_point_t &p) -> std::string {
        std::stringstream out;
        out << " [";
        switch (p.owner_tag) {
        case T::PLUGIN:
            out << "P";
            break;
        case T::SUPERVISOR:
            out << "S";
            break;
        case T::FOREIGN:
            out << "F";
            break;
        case T::ANONYMOUS:
            out << "A";
            break;
        case T::NOT_AVAILABLE:
            out << "NA";
            break;
        }
        out << "] m: " << demangle((const char *)p.handler->message_type) << ", addr: " << (void *)p.address.get()
            << " ";
        return out.str();
    };

    if (auto m = dynamic_cast<message::unsubscription_t *>(message); m) {
        level = 9;
        info += dump_point(m->payload.point);
    } else if (auto m = dynamic_cast<message::subscription_t *>(message); m) {
        level = 9;
        info += dump_point(m->payload.point);
    } else if (auto m = dynamic_cast<message::unsubscription_external_t *>(message); m) {
        level = 9;
        info += dump_point(m->payload.point);
    } else if (auto m = dynamic_cast<message::external_subscription_t *>(message); m) {
        level = 9;
        info += dump_point(m->payload.point);
    } else if (auto m = dynamic_cast<message::commit_unsubscription_t *>(message); m) {
        level = 9;
        info += dump_point(m->payload.point);
    }

    if (level > threshold)
        return "";
    return info;
}

void inspected_local_delivery_t::delivery(message_ptr_t &message,
                                          const subscription_t::joint_handlers_t &local_recipients) noexcept {
    auto var = std::getenv("ROTOR_INSPECT_DELIVERY");
    if (var) {
        int threshold = atoi(var);
        auto dump = identify(message.get(), threshold);
        if (dump.size() > 0) {
            std::cout << ">> " << dump << " for " << message->address.get() << "\n";
        }
    }
    local_delivery_t::delivery(message, local_recipients);
}
