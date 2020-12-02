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
    : actor_base_t(config), last_req_id{0}, subscription_map(*this), parent{config.supervisor}, manager{nullptr},
      create_registry(config.create_registry), synchronize_start(config.synchronize_start),
      registry_address(config.registry_address), policy{config.policy} {
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
        request<payload::initialize_actor_t>(address).send(init_timeout);
    }
    if (create_registry) {
        auto actor = create_actor<registry_t>().init_timeout(init_timeout).shutdown_timeout(shutdown_timeout).finish();
        registry_address = actor->address;
    }
    if (parent && !registry_address) {
        registry_address = parent->registry_address;
    }
}

void supervisor_t::do_shutdown() noexcept {
    if (state < state_t::SHUTTING_DOWN) {
        auto upstream_sup = parent ? parent : this;
        send<payload::shutdown_trigger_t>(upstream_sup->address, address);
    }
}

subscription_info_ptr_t supervisor_t::subscribe(const handler_ptr_t &handler, const address_ptr_t &addr,
                                                const actor_base_t *owner_ptr, owner_tag_t owner_tag) noexcept {
    subscription_point_t point(handler, addr, owner_ptr, owner_tag);
    auto sub_info = subscription_map.materialize(point);

    if (sub_info->internal_address) {
        send<payload::subscription_confirmation_t>(handler->actor_ptr->address, point);
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

void supervisor_t::on_child_init(actor_base_t *, const std::error_code &) noexcept {}

void supervisor_t::on_child_shutdown(actor_base_t *, const std::error_code &) noexcept {
    if (state == state_t::SHUTTING_DOWN) {
        shutdown_continue();
    }
}

void supervisor_t::intercept(message_ptr_t &, const void *, const continuation_t &cont) noexcept { cont(); }

void supervisor_t::on_request_trigger(request_id_t timer_id, bool cancelled) noexcept {
    auto it = request_map.find(timer_id);
    if (it != request_map.end()) {
        if (!cancelled) {
            auto &request_curry = it->second;
            message_ptr_t &request = request_curry.request_message;
            auto ec = make_error_code(error_code_t::request_timeout);
            auto timeout_message = request_curry.fn(request_curry.origin, *request, std::move(ec));
            put(std::move(timeout_message));
        }
        request_map.erase(it);
    }
}

void supervisor_t::discard_request(request_id_t request_id) noexcept {
    assert(request_map.find(request_id) != request_map.end());
    cancel_timer(request_id);
    request_map.erase(request_id);
}

void supervisor_t::shutdown_finish() noexcept {
    assert(request_map.size() == 0);
    actor_base_t::shutdown_finish();
}
