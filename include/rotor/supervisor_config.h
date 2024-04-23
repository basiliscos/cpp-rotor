#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <atomic>
#include "policy.h"
#include "actor_config.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace rotor {

/** \struct supervisor_config_t
 *  \brief base supervisor config, which holds shutdown timeout value
 */
struct supervisor_config_t : actor_config_t {

    using actor_config_t::actor_config_t;

    /** \brief how to behave if child-actor fails */
    supervisor_policy_t policy = supervisor_policy_t::shutdown_self;

    /** \brief whether the registry actor should be instantiated by supervisor */
    bool create_registry = false;

    /** \brief whether supervisor should wait until all actors confirmed
     * initialization, and only then send start signal to all of them */
    bool synchronize_start = false;

    /** \brief use the specified address of a registry
     *
     * Can be useful if an registry was created on the different supervisors
     * hierarchy
     */
    address_ptr_t registry_address;

    /** \brief initial queue size for inbound messages. Makes sense only for
     *  root/leader supervisor */
    size_t inbound_queue_size = 64;

    /**
     * \brief How much time it will spend in polling inbound queue before switching into
     * sleep mode (i.e. waiting external messages).
     *
     * This value might affect I/O latency on event-loops, so set it to zero to
     * minimizer I/O latency on event-loops by the rising cross-thread rotor messaging
     * latency.
     *
     */
    pt::time_duration poll_duration = pt::millisec{1};

    /** \brief pointer to atomic shutdown flag for polling (optional)
     *
     *  When it is set, supervisor will periodically check that the flag
     *  is set and then shut self down.
     *
     */
    const std::atomic_bool *shutdown_flag = nullptr;

    /** \brief the period for checking atomic shutdown flag */
    pt::time_duration shutdown_poll_frequency = pt::millisec{100};
};

/** \brief CRTP supervisor config builder */
template <typename Supervisor> struct supervisor_config_builder_t : actor_config_builder_t<Supervisor> {
    /** \brief final builder class */
    using builder_t = typename Supervisor::template config_builder_t<Supervisor>;

    /** \brief parent config builder */
    using parent_t = actor_config_builder_t<Supervisor>;

    using parent_t::parent_t;

    /** \brief defines actor's startup policy */
    builder_t &&policy(supervisor_policy_t policy_) && noexcept {
        parent_t::config.policy = policy_;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

    /** \brief instructs supervisor to create an registry */
    builder_t &&create_registry(bool value = true) && noexcept {
        parent_t::config.create_registry = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

    /** \brief instructs supervisor to synchronize start on it's children actors */
    builder_t &&synchronize_start(bool value = true) && noexcept {
        parent_t::config.synchronize_start = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

    /** \brief injects external registry address */
    builder_t &&registry_address(const address_ptr_t &value) && noexcept {
        parent_t::config.registry_address = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

    /** \brief initial queue size for inbound messages. Makes sense only for
     *  root/leader supervisor */
    builder_t &&inbound_queue_size(size_t value) && {
        parent_t::config.inbound_queue_size = value;
        return std::move(*static_cast<builder_t *>(this));
    }

    /** \brief how much time spend in active inbound queue polling */
    builder_t &&poll_duration(const pt::time_duration &value) && {
        parent_t::config.poll_duration = value;
        return std::move(*static_cast<builder_t *>(this));
    }

    /** \brief atomic shutdown flag and the period for polling it
     *
     * The thread-safe way to shutdown supervisor even when compiled with
     * `BUILD_THREAD_UNSAFE` option. Might be useful in single-threaded
     * applications.
     *
     * For more reactive behavior in thread-safe environment the
     * `supervisor::shutdown()` should be used.
     *
     */
    builder_t &&shutdown_flag(const std::atomic_bool &value, const pt::time_duration &interval) && {
        parent_t::config.shutdown_flag = &value;
        parent_t::config.shutdown_poll_frequency = interval;
        return std::move(*static_cast<builder_t *>(this));
    }

    virtual bool validate() noexcept {
        bool r = parent_t::validate();
        if (r) {
            r = !(parent_t::config.registry_address && parent_t::config.create_registry);
        }
        return r;
    }
};

} // namespace rotor

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
