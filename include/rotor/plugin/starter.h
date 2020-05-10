#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"

namespace rotor::internal {

struct starter_plugin_t: public plugin_t {
    using plugin_t::plugin_t;

    virtual void activate(actor_base_t* actor) noexcept override;
    void on_start(message::start_trigger_t& message) noexcept;
};

}
