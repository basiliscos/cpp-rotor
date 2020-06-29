//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "rotor/registry.h"
#include <assert.h>
using namespace rotor;

supervisor_t::supervisor_t(supervisor_config_t &config)
    : actor_base_t(config), parent{config.supervisor},
      subscription_support{nullptr}, manager{nullptr}, policy{config.policy}, last_req_id{1}, subscription_map(*this),
      create_registry(config.create_registry), registry_address(config.registry_address)
{
    if (!supervisor) {
        supervisor = this;
    }
    supervisor = this;
}

address_ptr_t supervisor_t::make_address() noexcept {
    auto root_sup = this;
    while (root_sup->parent) {
        root_sup = root_sup->parent;
    }
    return instantiate_address(root_sup);
}

address_ptr_t supervisor_t::instantiate_address(const void *locality) noexcept {
    return new address_t{*this, locality};
}

void supervisor_t::do_initialize(system_context_t *ctx) noexcept {
    context = ctx;
    actor_base_t::do_initialize(ctx);
    // do self-bootstrap
    if (!parent) {
        request<payload::initialize_actor_t>(address, address).send(shutdown_timeout);
    }
    if (create_registry) {
        auto actor = create_actor<registry_t>()
                .init_timeout(init_timeout)
                .shutdown_timeout(shutdown_timeout).finish();
        registry_address = actor->get_address();
    }
    if (parent && !registry_address) {
        registry_address = parent->registry_address;
    }
}

void supervisor_t::do_shutdown() noexcept {
    auto upstream_sup = parent ? parent : this;
    send<payload::shutdown_trigger_t>(upstream_sup->get_address(), address);
}

subscription_info_ptr_t supervisor_t::subscribe(const handler_ptr_t &handler, const address_ptr_t &addr,
                                                const actor_base_t *owner_ptr, owner_tag_t owner_tag) noexcept {
    subscription_point_t point(handler, addr, owner_ptr, owner_tag);
    auto sub_info = subscription_map.materialize(point);

    if (sub_info->internal_address) {
        send<payload::subscription_confirmation_t>(handler->actor_ptr->get_address(), point);
    } else {
        send<payload::external_subscription_t>(addr->supervisor.address, point);
    }

    if (sub_info->internal_handler) {
        handler->actor_ptr->lifetime->initate_subscription(sub_info);
    }

    return sub_info;
}

void supervisor_t::commit_unsubscription(const subscription_info_ptr_t &info) noexcept {
    subscription_map.forget(info);
}

void supervisor_t::shutdown_finish() noexcept {
    // address_mapping.clear(*this);
    actor_base_t::shutdown_finish();
}

void supervisor_t::on_timer_trigger(timer_id_t timer_id) {
    auto it = request_map.find(timer_id);
    if (it != request_map.end()) {
        auto &request_curry = it->second;
        message_ptr_t &request = request_curry.request_message;
        auto ec = make_error_code(error_code_t::request_timeout);
        auto timeout_message = request_curry.fn(request_curry.reply_to, *request, std::move(ec));
        put(std::move(timeout_message));
        request_map.erase(it);
    }
}
