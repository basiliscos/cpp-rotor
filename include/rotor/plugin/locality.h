#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"

namespace rotor::internal {

struct locality_plugin_t: public plugin_t {
    using plugin_t::plugin_t;

    static const void* class_identity;
    const void* identity() const noexcept override;

    void activate(actor_base_t* actor) noexcept override;
};

}
