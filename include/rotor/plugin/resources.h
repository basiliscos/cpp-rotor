#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"
#include <vector>

namespace rotor::plugin {

using resource_id_t = std::size_t;

struct resources_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;
    bool handle_init(message::init_request_t *message) noexcept override;
    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;

    virtual void acquire(resource_id_t = 0) noexcept;
    virtual std::uint32_t has(resource_id_t = 0) noexcept;
    virtual bool release(resource_id_t = 0) noexcept;
    virtual bool has_any() noexcept;

    template <typename T> auto &access() noexcept;

  private:
    using Resources = std::vector<std::uint32_t>;
    Resources resources;
    bool configured = false;
};

} // namespace rotor::plugin
