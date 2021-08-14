//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
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

std::string inspected_local_delivery_t::identify(const message_base_t *message, std::int32_t threshold) noexcept {
    using boost::core::demangle;
    using T = owner_tag_t;
    std::string info = demangle((const char *)message->type_index);
    std::int32_t level = 5;
    auto dump_point = [](const subscription_point_t &p) -> std::string {
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
        }
        out << "] m: " << demangle((const char *)p.handler->message_type) << ", addr: " << (void *)p.address.get()
            << " ";
        return out.str();
    };

    if (auto m = dynamic_cast<const message::unsubscription_t *>(message); m) {
        level = 9;
        info += dump_point(m->payload.point);
    } else if (auto m = dynamic_cast<const message::subscription_t *>(message); m) {
        level = 9;
        info += dump_point(m->payload.point);
    } else if (auto m = dynamic_cast<const message::unsubscription_external_t *>(message); m) {
        level = 9;
        info += dump_point(m->payload.point);
    } else if (auto m = dynamic_cast<const message::external_subscription_t *>(message); m) {
        level = 9;
        info += dump_point(m->payload.point);
    } else if (auto m = dynamic_cast<const message::commit_unsubscription_t *>(message); m) {
        level = 9;
        info += dump_point(m->payload.point);
    } else if (auto m = dynamic_cast<const message::deregistration_service_t *>(message); m) {
        level = 1;
        info += ", service = ";
        info += m->payload.service_name;
    } else if (auto m = dynamic_cast<const message::registration_request_t *>(message); m) {
        level = 1;
        info += ", name = ";
        info += m->payload.request_payload.service_name;
    } else if (auto m = dynamic_cast<const message::registration_response_t *>(message); m) {
        level = 1;
        info += ", name = ";
        info += m->payload.req->payload.request_payload.service_name;
        auto &ee = m->payload.ee;
        if (ee) {
            info += ", failure(!) = ";
            info += ee->message() + " ";
        } else {
            info += ", success; ";
        }
    } else if (auto m = dynamic_cast<const message::discovery_request_t *>(message); m) {
        level = 1;
        info += ", service = ";
        info += m->payload.request_payload.service_name;
    } else if (auto m = dynamic_cast<const message::discovery_promise_t *>(message); m) {
        level = 1;
        info += ", service = ";
        info += m->payload.request_payload.service_name;
    } else if (auto m = dynamic_cast<const message::discovery_future_t *>(message); m) {
        level = 1;
        std::stringstream out;
        out << ", service = " << m->payload.req->payload.request_payload.service_name;
        auto &ee = m->payload.ee;
        if (ee) {
            out << ", failure = " << ee->message() << " ";
        } else {
            auto &addr = m->payload.res.service_addr;
            out << ", success, addr =  " << addr.get() << " ";
        }
        info += out.str();
    } else if (auto m = dynamic_cast<const message::discovery_response_t *>(message); m) {
        level = 1;
        std::stringstream out;
        out << ", service = " << m->payload.req->payload.request_payload.service_name;
        auto &ee = m->payload.ee;
        if (ee) {
            out << ", failure = " << ee->message() << " ";
        } else {
            auto &addr = m->payload.res.service_addr;
            out << ", success, addr =  " << addr.get() << " ";
        }
        info += out.str();
    } else if (auto m = dynamic_cast<const message::link_response_t *>(message); m) {
        level = 2;
        std::stringstream out;
        out << ", service addr = " << m->payload.req->payload.origin.get();
        auto &ee = m->payload.ee;
        if (ee) {
            out << ", failure(!) = " << ee->message() + " ";
        } else {
            out << ", success; ";
        }
        info += out.str();
    } else if (auto m = dynamic_cast<const message::shutdown_trigger_t *>(message); m) {
        level = 0;
        std::stringstream out;
        out << ", target = " << ((void *)m->payload.actor_address.get())
            << ", reason = " << m->payload.reason->message();
        info += out.str();
    }

    if (level > threshold)
        return "";
    return info;
}

static void dump_message(const char *prefix, const message_ptr_t &message) noexcept {
    auto var = std::getenv("ROTOR_INSPECT_DELIVERY");
    if (var) {
        int threshold = atoi(var);
        auto dump = inspected_local_delivery_t::identify(message.get(), threshold);
        if (dump.size() > 0) {
            std::cout << prefix << dump << " for " << message->address.get() << "\n";
        }
    }
}

void inspected_local_delivery_t::delivery(message_ptr_t &message,
                                          const subscription_t::joint_handlers_t &local_recipients) noexcept {
    dump_message(">> ", message);
    local_delivery_t::delivery(message, local_recipients);
}

void inspected_local_delivery_t::discard(message_ptr_t &message) noexcept { dump_message("<DISCARDED> ", message); }
