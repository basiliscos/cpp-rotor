#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"

namespace rotor::internal {

struct foreigners_support_plugin_t : public plugin_t {
    using plugin_t::plugin_t;

    static const void *class_identity;
    const void *identity() const noexcept override;
    void activate(actor_base_t *actor) noexcept override;
    void deactivate() noexcept override;

    virtual void on_call(message::handler_call_t &message) noexcept;
    virtual void on_unsubscription(message::commit_unsubscription_t &message) noexcept;
    virtual void on_subscription_external(message::external_subscription_t &message) noexcept;

  private:
    subscription_container_t foreign_points;
};

} // namespace rotor::internal
