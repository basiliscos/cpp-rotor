#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin.h"

namespace rotor::internal {

struct init_shutdown_plugin_t: public plugin_t {
    using plugin_t::plugin_t;

    virtual bool is_complete_for(slot_t slot) noexcept override;
    virtual void activate(actor_base_t* actor) noexcept override;
    virtual void deactivate() noexcept override;
    virtual void confirm_init() noexcept;

    virtual void on_init(message::init_request_t&) noexcept;
    virtual void on_shutdown(message::shutdown_request_t&) noexcept;
    virtual bool handle_shutdown(message::shutdown_request_t&) noexcept override;

    /** \brief suspended init request message */
    intrusive_ptr_t<message::init_request_t> init_request;
};


}
