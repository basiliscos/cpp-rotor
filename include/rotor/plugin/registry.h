#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"
#include "link_client.h"
#include "rotor/error_code.h"
#include <string>
#include <unordered_map>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace rotor::plugin {

/** \struct registry_plugin_t
 *
 * \brief handy access to {@link registry_t}, for name registration and discovery
 *
 * Can use {@link registry_t} to register name/address during actor init phase
 * and perform the reverse operation to deregister name/address during shutdown phase.
 *
 * Simialary the {@link registry_t} can be used to discover name in the registry
 * and link the current actor to the target address during init phase; there is
 * no reverse operation during shutdown phase, because unlinking will be handled
 * by {@link plugin::link_client_plugin_t} plugin.
 *
 */
struct ROTOR_API registry_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    /** \brief phase for each discovery task: discovering or linking */
    enum class phase_t { discovering, linking };

    /** \struct discovery_task_t
     * \brief helper class to invoke callback upon address discovery
     */
    struct ROTOR_API discovery_task_t {
        /** \brief callback for the discovery progress */
        using callback_t = std::function<void(phase_t phase, const extended_error_ptr_t &)>;

        /** \brief stat of the discovery task */
        enum class state_t { PASSIVE, DISCOVERING, LINKING, OPERATIONAL, CANCELLING };

        /** \brief sets that linking should be performed on operational-only discovered address */
        discovery_task_t &link(bool operational_only_ = true) noexcept {
            link_on_discovery = true;
            operational_only = operational_only_;
            return *this;
        }

        /** \brief discovery progress callback setter */
        template <typename Callback> void callback(Callback &&cb) noexcept {
            task_callback = std::forward<Callback>(cb);
        }

        /** \brief generic non-public methods accessor */
        template <typename T, typename... Args> auto access(Args... args) noexcept;

      private:
        discovery_task_t(registry_plugin_t &plugin_, address_ptr_t *address_, std::string service_name_, bool delayed_)
            : plugin{&plugin_},
              address(address_), service_name{service_name_}, delayed{delayed_}, state{state_t::PASSIVE} {}
        operator bool() const noexcept { return address; }

        void do_discover() noexcept;
        void on_discovery(address_ptr_t *service_addr, const extended_error_ptr_t &ec) noexcept;
        bool do_cancel() noexcept;
        void post_discovery(const extended_error_ptr_t &ec) noexcept;

        // allow implicit copy-assignment operator
        registry_plugin_t *plugin;
        address_ptr_t *address;
        std::string service_name;
        bool delayed;
        state_t state;
        request_id_t request_id = 0;
        callback_t task_callback;
        bool link_on_discovery = false;
        bool operational_only = false;

        friend struct registry_plugin_t;
    };

    /** The plugin unique identity to allow further static_cast'ing*/
    static const void *class_identity;

    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;

    /** \brief reaction on registration response */
    virtual void on_registration(message::registration_response_t &) noexcept;

    /** \brief reaction on discovery response */
    virtual void on_discovery(message::discovery_response_t &) noexcept;

    /** \brief reaction on discovery future */
    virtual void on_future(message::discovery_future_t &message) noexcept;

    /** \brief enqueues name/address registration
     *
     * It links with registry actor first upon demand, and then sends to it
     * name registration request(s).
     */
    virtual void register_name(const std::string &name, const address_ptr_t &address) noexcept;

    /** \brief creates name discovery task
     *
     * The address pointer is the place, where the discovered address should be stored.
     *
     * The `delayed` means: if the name is missing in the registry, do not response
     * with error (which will cause shutdown of client), but wait until the name be
     * registered, and only then reply with found address. In other words: instead
     * of sending discovery request, it will send discovery future.
     *
     */
    virtual discovery_task_t &discover_name(const std::string &name, address_ptr_t &address,
                                            bool delayed = false) noexcept;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;
    bool handle_init(message::init_request_t *message) noexcept override;

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

    /** \brief service name to task mapping */
    using discovery_map_t = std::unordered_map<std::string, discovery_task_t>;

  private:
    enum class state_t { REGISTERING, LINKING, OPERATIONAL, UNREGISTERING };
    struct register_info_t {
        address_ptr_t address;
        state_t state;
    };
    using register_map_t = std::unordered_map<std::string, register_info_t>;
    using names_t = std::vector<std::string>;
    using aliases_map_t = std::unordered_map<address_ptr_t, names_t>;

    enum plugin_state_t : std::uint32_t {
        CONFIGURED = 1 << 0,
        LINKING = 1 << 1,
        LINKED = 1 << 2,
    };
    std::uint32_t plugin_state = 0;

    register_map_t register_map;
    discovery_map_t discovery_map;
    aliases_map_t aliases_map;

    void link_registry() noexcept;
    void on_link(const extended_error_ptr_t &ec) noexcept;
    bool has_registering() noexcept;
    virtual void continue_init(const error_code_t &possible_ec, const extended_error_ptr_t &root_ec) noexcept;
};

} // namespace rotor::plugin

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
