#pragma once
//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <functional>

namespace rotor {

struct actor_base_t;
struct supervisor_t;

struct behaviour_t {
    using fn_t = std::function<void()>;
    inline void next() noexcept { fn(); }
    virtual ~behaviour_t();
    virtual void init() noexcept = 0;

    fn_t fn;
};

struct actor_shutdown_t : public behaviour_t {
    actor_shutdown_t(actor_base_t &actor);

    virtual void init() noexcept override;
    virtual void unsubscribe_self() noexcept;
    virtual void confirm_request() noexcept;
    virtual void commit_state() noexcept;
    virtual void cleanup() noexcept;
    actor_base_t &actor;
};

struct supervisor_shutdown_t : public actor_shutdown_t {
    supervisor_shutdown_t(supervisor_t &supervisor);
    virtual void init() noexcept override;
    virtual void shutdown_children() noexcept;
};

} // namespace rotor
