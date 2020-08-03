#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"

namespace rotor::plugin {

template <typename Plugin> struct subscriber_plugin_t : public Plugin {
    using Plugin::Plugin;

    template <typename Handler> handler_ptr_t subscribe_actor(Handler &&handler) noexcept;
    template <typename Handler> handler_ptr_t subscribe_actor(Handler &&handler, const address_ptr_t &addr) noexcept;

    void deactivate() noexcept override {
        tracked.clear();
        return plugin_base_t::deactivate();
    }

  protected:
    subscription_container_t tracked;
    bool continue_init = false;
};

} // namespace rotor::plugin
