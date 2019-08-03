#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "rotor/ev/supervisor_config.h"
#include "rotor/ev/system_context_ev.h"
#include "rotor/system_context.h"
#include <ev++.h>
#include <mutex>

namespace rotor {
namespace ev {

struct supervisor_ev_t : public supervisor_t {
    supervisor_ev_t(supervisor_ev_t *parent, const supervisor_config_t &config);
    ~supervisor_ev_t();

    /** \brief creates an actor by forwaring `args` to it
     *
     * The newly created actor belogs to the wx supervisor / wx event loop
     */
    template <typename Actor, typename... Args> intrusive_ptr_t<Actor> create_actor(Args... args) {
        return make_actor<Actor>(*this, std::forward<Args>(args)...);
    }

    virtual void start() noexcept override;
    virtual void shutdown() noexcept override;
    virtual void enqueue(message_ptr_t message) noexcept override;
    virtual void start_shutdown_timer() noexcept override;
    virtual void cancel_shutdown_timer() noexcept override;
    virtual void confirm_shutdown() noexcept override;

    inline struct ev_loop *get_loop() noexcept { return config.loop; }

    /** \brief returns pointer to the wx system context */
    inline system_context_ev_t *get_context() noexcept { return static_cast<system_context_ev_t *>(context); }

  protected:
    static void async_cb(EV_P_ ev_async *w, int revents) noexcept;

    virtual void on_async() noexcept;

    /** \brief timeout value, EV event loop pointer and loop ownership flag */
    supervisor_config_t config;
    ev_async async_watcher;
    ev_timer shutdown_watcher;

    std::mutex inbound_mutex;
    queue_t inbound;
};

} // namespace ev
} // namespace rotor
