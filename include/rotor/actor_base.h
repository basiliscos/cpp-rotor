#pragma once

#include "address.hpp"
#include "messages.hpp"
#include <unordered_map>
#include <vector>

namespace rotor {

struct supervisor_t;
struct system_context_t;
struct handler_base_t;
using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

struct actor_base_t : public arc_base_t<actor_base_t> {
    struct subscription_request_t {
        handler_ptr_t handler;
        address_ptr_t address;
    };

    using subscription_points_t = std::unordered_map<address_ptr_t, handler_ptr_t>;

    actor_base_t(supervisor_t &supervisor_);
    virtual ~actor_base_t();

    virtual void do_initialize() noexcept;
    virtual void do_shutdown() noexcept;
    virtual address_ptr_t create_address() noexcept;

    void remember_subscription(const subscription_request_t &req) noexcept;
    void forget_subscription(const subscription_request_t &req) noexcept;

    inline address_ptr_t get_address() const noexcept { return address; }
    inline supervisor_t &get_supevisor() const noexcept { return supervisor; }
    inline subscription_points_t &get_subscription_points() noexcept { return points; }

    virtual void on_initialize(message_t<payload::initialize_actor_t> &) noexcept;
    virtual void on_start(message_t<payload::start_actor_t> &) noexcept;
    virtual void on_shutdown(message_t<payload::shutdown_request_t> &) noexcept;

    template <typename M, typename... Args> void send(const address_ptr_t &addr, Args &&... args);

    template <typename Handler> void subscribe(Handler &&h);

  protected:
    supervisor_t &supervisor;
    address_ptr_t address;
    subscription_points_t points;
};

using actor_ptr_t = intrusive_ptr_t<actor_base_t>;

} // namespace rotor
