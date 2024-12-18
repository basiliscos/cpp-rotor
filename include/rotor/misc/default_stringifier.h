#pragma once

//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "../messages.hpp"
#include "../message_stringifier.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace rotor {

struct subscription_point_t;

namespace misc {

/** \struct default_stringifier_t
 *  \brief Default stringifier of rotor messages
 *
 * The custom message stringifier should override the `try_visit` method
 * and do message stringification of own custom messages, and only then
 * call `try_visit` of the `default_stringifier`.
 *
 * The stringifier is potentially slow and should be used for
 * debugging or problems identification.
 *
 */

struct ROTOR_API default_stringifier_t : message_stringifier_t,
                                         protected message_visitor_t,
                                         protected message::unsubscription_t::visitor_t,
                                         protected message::unsubscription_external_t::visitor_t,
                                         protected message::subscription_t::visitor_t,
                                         protected message::external_subscription_t::visitor_t,
                                         protected message::commit_unsubscription_t::visitor_t,
                                         protected message::handler_call_t::visitor_t,
                                         protected message::init_request_t::visitor_t,
                                         protected message::init_response_t::visitor_t,
                                         protected message::start_trigger_t::visitor_t,
                                         protected message::shutdown_trigger_t::visitor_t,
                                         protected message::shutdown_request_t::visitor_t,
                                         protected message::shutdown_response_t::visitor_t,
                                         protected message::create_actor_t::visitor_t,
                                         protected message::spawn_actor_t::visitor_t,
                                         protected message::registration_request_t::visitor_t,
                                         protected message::registration_response_t::visitor_t,
                                         protected message::deregistration_notify_t::visitor_t,
                                         protected message::deregistration_service_t::visitor_t,
                                         protected message::discovery_request_t::visitor_t,
                                         protected message::discovery_response_t::visitor_t,
                                         protected message::discovery_promise_t::visitor_t,
                                         protected message::discovery_future_t::visitor_t,
                                         protected message::discovery_cancel_t::visitor_t,
                                         protected message::link_request_t::visitor_t,
                                         protected message::link_response_t::visitor_t,
                                         protected message::unlink_notify_t::visitor_t,
                                         protected message::unlink_request_t::visitor_t,
                                         protected message::unlink_response_t::visitor_t {
    void stringify_to(std::ostream &, const message_base_t &) const override;
    bool try_visit(const message_base_t &message, void *context) const override;

  private:
    void on(const message::unsubscription_t &, void *) override;
    void on(const message::unsubscription_external_t &, void *) override;
    void on(const message::subscription_t &, void *) override;
    void on(const message::external_subscription_t &, void *) override;
    void on(const message::commit_unsubscription_t &, void *) override;
    void on(const message::handler_call_t &, void *) override;
    void on(const message::init_request_t &, void *) override;
    void on(const message::init_response_t &, void *) override;
    void on(const message::start_trigger_t &, void *) override;
    void on(const message::shutdown_trigger_t &, void *) override;
    void on(const message::shutdown_request_t &, void *) override;
    void on(const message::shutdown_response_t &, void *) override;
    void on(const message::create_actor_t &, void *) override;
    void on(const message::spawn_actor_t &, void *) override;
    void on(const message::registration_request_t &, void *) override;
    void on(const message::registration_response_t &, void *) override;
    void on(const message::deregistration_notify_t &, void *) override;
    void on(const message::deregistration_service_t &, void *) override;
    void on(const message::discovery_request_t &, void *) override;
    void on(const message::discovery_response_t &, void *) override;
    void on(const message::discovery_promise_t &, void *) override;
    void on(const message::discovery_future_t &, void *) override;
    void on(const message::discovery_cancel_t &, void *) override;
    void on(const message::link_request_t &, void *) override;
    void on(const message::link_response_t &, void *) override;
    void on(const message::unlink_notify_t &, void *) override;
    void on(const message::unlink_request_t &, void *) override;
    void on(const message::unlink_response_t &, void *) override;
};

} // namespace misc
} // namespace rotor

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
