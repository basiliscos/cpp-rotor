//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/message.h"
#include "rotor/handler.h"
#include "rotor/misc/default_stringifier.h"

#include <boost/core/demangle.hpp>

namespace rotor::misc {

std::ostream &operator<<(std::ostream &stream, const subscription_point_t &p) {
    using boost::core::demangle;
    using T = owner_tag_t;
    stream << "{";
    switch (p.owner_tag) {
    case T::PLUGIN:
        stream << "P";
        break;
    case T::SUPERVISOR:
        stream << "S";
        break;
    case T::FOREIGN:
        stream << "F";
        break;
    case T::ANONYMOUS:
        stream << "A";
        break;
    }
    return stream << " : " << (void *)p.address.get() << " / " << demangle((const char *)p.handler->message_type())
                  << "}";
}

bool default_stringifier_t::try_visit(const message_base_t &message) const {
    auto &self = const_cast<default_stringifier_t &>(*this);
    if (message.type_index == message::unsubscription_t::message_type) {
        self.on(static_cast<const message::unsubscription_t &>(message));
    } else if (message.type_index == message::unsubscription_external_t::message_type) {
        self.on(static_cast<const message::unsubscription_external_t &>(message));
    } else if (message.type_index == message::subscription_t::message_type) {
        self.on(static_cast<const message::subscription_t &>(message));
    } else if (message.type_index == message::external_subscription_t::message_type) {
        self.on(static_cast<const message::external_subscription_t &>(message));
    } else if (message.type_index == message::commit_unsubscription_t::message_type) {
        self.on(static_cast<const message::commit_unsubscription_t &>(message));
    } else if (message.type_index == message::handler_call_t::message_type) {
        self.on(static_cast<const message::handler_call_t &>(message));
    } else if (message.type_index == message::init_request_t::message_type) {
        self.on(static_cast<const message::init_request_t &>(message));
    } else if (message.type_index == message::init_response_t::message_type) {
        self.on(static_cast<const message::init_response_t &>(message));
    } else if (message.type_index == message::start_trigger_t::message_type) {
        self.on(static_cast<const message::start_trigger_t &>(message));
    } else if (message.type_index == message::shutdown_trigger_t::message_type) {
        self.on(static_cast<const message::shutdown_trigger_t &>(message));
    } else if (message.type_index == message::shutdown_request_t::message_type) {
        self.on(static_cast<const message::shutdown_request_t &>(message));
    } else if (message.type_index == message::shutdown_response_t::message_type) {
        self.on(static_cast<const message::shutdown_response_t &>(message));
    } else if (message.type_index == message::create_actor_t::message_type) {
        self.on(static_cast<const message::create_actor_t &>(message));
    } else if (message.type_index == message::spawn_actor_t::message_type) {
        self.on(static_cast<const message::spawn_actor_t &>(message));
    } else if (message.type_index == message::registration_request_t::message_type) {
        self.on(static_cast<const message::registration_request_t &>(message));
    } else if (message.type_index == message::registration_response_t::message_type) {
        self.on(static_cast<const message::registration_response_t &>(message));
    } else if (message.type_index == message::deregistration_notify_t::message_type) {
        self.on(static_cast<const message::deregistration_notify_t &>(message));
    } else if (message.type_index == message::deregistration_service_t::message_type) {
        self.on(static_cast<const message::deregistration_service_t &>(message));
    } else if (message.type_index == message::discovery_request_t::message_type) {
        self.on(static_cast<const message::discovery_request_t &>(message));
    } else if (message.type_index == message::discovery_response_t::message_type) {
        self.on(static_cast<const message::discovery_response_t &>(message));
    } else if (message.type_index == message::discovery_promise_t::message_type) {
        self.on(static_cast<const message::discovery_promise_t &>(message));
    } else if (message.type_index == message::discovery_future_t::message_type) {
        self.on(static_cast<const message::discovery_future_t &>(message));
    } else if (message.type_index == message::discovery_cancel_t::message_type) {
        self.on(static_cast<const message::discovery_cancel_t &>(message));
    } else if (message.type_index == message::link_request_t::message_type) {
        self.on(static_cast<const message::link_request_t &>(message));
    } else if (message.type_index == message::link_response_t::message_type) {
        self.on(static_cast<const message::link_response_t &>(message));
    } else if (message.type_index == message::unlink_notify_t::message_type) {
        self.on(static_cast<const message::unlink_notify_t &>(message));
    } else if (message.type_index == message::unlink_request_t::message_type) {
        self.on(static_cast<const message::unlink_request_t &>(message));
    } else if (message.type_index == message::unlink_response_t::message_type) {
        self.on(static_cast<const message::unlink_response_t &>(message));
    } else {
        return false;
    }
    return true;
}

void default_stringifier_t::stringify(std::stringstream &stream, const message_base_t &message) const {
    using boost::core::demangle;
    this->stream = &stream;
    if (!try_visit(message)) {
        stream << "[?] " << demangle((const char *)message.type_index);
    }
    stream << " => " << message.address;
}

void default_stringifier_t::on(const message::unsubscription_t &message) {
    (*stream) << "r::unsubscription " << message.payload.point;
}

void default_stringifier_t::on(const message::unsubscription_external_t &message) {
    (*stream) << "r::unsubscription_external " << message.payload.point;
}

void default_stringifier_t::on(const message::subscription_t &message) {
    (*stream) << "r::subscription " << message.payload.point;
}

void default_stringifier_t::on(const message::external_subscription_t &message) {
    (*stream) << "r::external_subscription " << message.payload.point;
}

void default_stringifier_t::on(const message::commit_unsubscription_t &message) {
    (*stream) << "r::commit_unsubscription " << message.payload.point;
}

void default_stringifier_t::on(const message::handler_call_t &message) {
    (*stream) << "r::handler_call {";
    stringify(*stream, *message.payload.orig_message);
    (*stream) << "} @ " << (const void *)message.payload.handler->actor_ptr;
}

void default_stringifier_t::on(const message::init_request_t &message) {
    (*stream) << "r::init_request #" << message.payload.id << " > " << message.address;
}

void default_stringifier_t::on(const message::init_response_t &message) {
    auto &ee = message.payload.ee;
    auto &p = message.payload.req->payload;
    (*stream) << "r::init_response #" << p.id << " > " << p.origin;
    if (ee) {
        (*stream) << ", fail:" << ee->message();
    }
}

void default_stringifier_t::on(const message::start_trigger_t &message) {
    (*stream) << "r::start_trigger @ " << message.address;
}

void default_stringifier_t::on(const message::shutdown_trigger_t &message) {
    auto &p = message.payload;
    (*stream) << "r::shutdown_trigger @ " << message.payload.actor_address;
    if (p.reason) {
        (*stream) << " : " << p.reason->message();
    }
}

void default_stringifier_t::on(const message::shutdown_request_t &message) {
    auto &p = message.payload.request_payload;
    (*stream) << "r::shutdown_request # " << message.payload.id << " > " << message.payload.origin;
    if (p.reason) {
        (*stream) << " : " << p.reason->message();
    }
}

void default_stringifier_t::on(const message::shutdown_response_t &message) {
    auto &ee = message.payload.ee;
    auto &p = message.payload.req->payload;
    (*stream) << "r::shutdown_response # " << p.id << " > " << p.origin;
    if (ee) {
        (*stream) << ", fail:" << ee->message();
    }
}

void default_stringifier_t::on(const message::create_actor_t &message) {
    using boost::core::demangle;
    auto &actor = *(message.payload.actor);
    (*stream) << "r::create_actor @ " << message.address << " : " << demangle(typeid(actor).name());
}

void default_stringifier_t::on(const message::spawn_actor_t &message) {
    (*stream) << "r::spawn_actor @{ " << message.address << " -> " << message.payload.spawner_address << "}";
}

void default_stringifier_t::on(const message::registration_request_t &message) {
    auto &p = message.payload;
    (*stream) << "r::registration_request # " << p.id << " '" << p.request_payload.service_name << " @ "
              << p.request_payload.service_addr;
}

void default_stringifier_t::on(const message::registration_response_t &message) {
    auto &ee = message.payload.ee;
    auto &req = *message.payload.req;
    auto &p = req.payload.request_payload;
    (*stream) << "r::registration_response # " << req.payload.id << " '" << p.service_name << "' @ " << p.service_addr;
    if (ee) {
        (*stream) << ", fail:" << ee->message();
    }
}

void default_stringifier_t::on(const message::deregistration_notify_t &message) {
    (*stream) << "r::deregistration_notify @ " << message.payload.service_addr;
}

void default_stringifier_t::on(const message::deregistration_service_t &message) {
    (*stream) << "r::deregistration_service '" << message.payload.service_name << "'";
}

void default_stringifier_t::on(const message::discovery_request_t &message) {
    auto &p = message.payload;
    (*stream) << "r::discovery_request '" << p.request_payload.service_name << "'";
}

void default_stringifier_t::on(const message::discovery_response_t &message) {
    auto &ee = message.payload.ee;
    auto &p = message.payload.req->payload;
    (*stream) << "r::discovery_response '" << p.request_payload.service_name << "'";
    if (ee) {
        (*stream) << ", fail:" << ee->message();
    } else {
        (*stream) << " -> @ " << message.payload.res.service_addr;
    }
}

void default_stringifier_t::on(const message::discovery_promise_t &message) {
    auto &p = message.payload;
    (*stream) << "r::discovery_promise '" << p.request_payload.service_name << "'";
}

void default_stringifier_t::on(const message::discovery_future_t &message) {
    auto &ee = message.payload.ee;
    auto &p = message.payload.req->payload;
    (*stream) << "r::discovery_future '" << p.request_payload.service_name << "'";
    if (ee) {
        (*stream) << ", fail:" << ee->message();
    } else {
        (*stream) << " -> @ " << message.payload.res.service_addr;
    }
}

void default_stringifier_t::on(const message::discovery_cancel_t &message) {
    (*stream) << "r::discovery_cancel @ " << message.payload.source;
}

void default_stringifier_t::on(const message::link_request_t &message) {
    (*stream) << "r::link_request @ " << message.address << "-> @ " << message.payload.origin;
}

void default_stringifier_t::on(const message::link_response_t &message) {
    auto &ee = message.payload.ee;
    auto &p = message.payload.req;
    (*stream) << "r::link_response @ " << p->payload.origin;
    if (ee) {
        (*stream) << ", fail:" << ee->message();
    }
}

void default_stringifier_t::on(const message::unlink_notify_t &message) {
    (*stream) << "r::unlink_notify @ " << message.payload.client_addr;
}

void default_stringifier_t::on(const message::unlink_request_t &message) {
    (*stream) << "r::unlink_request @ " << message.payload.request_payload.server_addr;
}

void default_stringifier_t::on(const message::unlink_response_t &message) {
    auto &ee = message.payload.ee;
    auto &p = message.payload.req;
    (*stream) << "r::unlink_response @ " << p->payload.request_payload.server_addr;
    if (ee) {
        (*stream) << ", fail:" << ee->message();
    }
}

} // namespace rotor::misc
