#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"

namespace rotor::internal {

struct subscriber_plugin_t : public plugin_t {
    using plugin_t::plugin_t;

    template <typename Handler> void subscribe_actor(Handler &&handler) noexcept;
    template <typename Handler> void subscribe_actor(Handler &&handler, const address_ptr_t &addr) noexcept;

    void deactivate() noexcept override;

    struct to {
        struct address {};
        struct supervisor {};
    };

  protected:
    subscription_container_t tracked;
    bool continue_init = false;
};

} // namespace rotor::internal
