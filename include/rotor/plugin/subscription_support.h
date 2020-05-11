#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"

namespace rotor::internal {

struct subscription_support_plugin_t: public plugin_t {
    using plugin_t::plugin_t;

    bool activate(actor_base_t* actor) noexcept override;
    bool deactivate() noexcept override;

    virtual void on_call(message::handler_call_t& message) noexcept;
    virtual void on_unsubscription(message::commit_unsubscription_t& message) noexcept;
    virtual void on_unsubscription_external(message::external_subscription_t& message) noexcept;
};

}
