#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "plugin_base.h"
#include "link_client.h"
#include <string>
#include <unordered_map>

namespace rotor::plugin {

struct registry_plugin_t : public plugin_base_t {
    using plugin_base_t::plugin_base_t;

    enum class phase_t { discovering, linking };
    struct discovery_task_t {
        using callback_t = std::function<void(phase_t phase, const std::error_code &)>;

        discovery_task_t &link(bool operational_only_ = true) noexcept {
            link_on_discovery = true;
            operational_only = operational_only_;
            return *this;
        }

        template <typename Callback> void callback(Callback &&cb) noexcept {
            task_callback = std::forward<Callback>(cb);
        }

      private:
        discovery_task_t(registry_plugin_t &plugin_, address_ptr_t *address_, std::string service_name_, bool delayed_)
            : plugin{plugin_}, address(address_), service_name{service_name_}, delayed{delayed_} {}
        operator bool() const noexcept { return address; }

        void on_discovery(const std::error_code &ec) noexcept;
        void continue_init(const std::error_code &ec) noexcept;

        registry_plugin_t &plugin;
        address_ptr_t *address;
        std::string service_name;
        bool delayed;
        bool link_on_discovery = false;
        bool operational_only = false;
        callback_t task_callback;

        friend struct registry_plugin_t;
    };

    static const void *class_identity;
    const void *identity() const noexcept override;

    void activate(actor_base_t *actor) noexcept override;

    virtual void on_registration(message::registration_response_t &) noexcept;
    virtual void on_discovery(message::discovery_response_t &) noexcept;
    virtual void on_future(message::discovery_future_t &message) noexcept;

    virtual bool register_name(const std::string &name, const address_ptr_t &address) noexcept;
    virtual discovery_task_t &discover_name(const std::string &name, address_ptr_t &address,
                                            bool delayed = false) noexcept;

    bool handle_shutdown(message::shutdown_request_t *message) noexcept override;
    bool handle_init(message::init_request_t *message) noexcept override;

    template <typename T> auto &access() noexcept;

  private:
    template <typename Message> void process_discovery(Message &message) noexcept {
        auto &service = message.payload.req->payload.request_payload.service_name;
        auto &ec = message.payload.ec;
        auto it = discovery_map.find(service);
        assert(it != discovery_map.end());
        if (!ec) {
            *it->second.address = message.payload.res.service_addr;
        }
        it->second.on_discovery(ec);
    }

    enum class state_t { REGISTERING, LINKING, OPERATIONAL, UNREGISTERING };
    struct register_info_t {
        address_ptr_t address;
        state_t state;
    };
    using register_map_t = std::unordered_map<std::string, register_info_t>;
    using discovery_map_t = std::unordered_map<std::string, discovery_task_t>;

    enum plugin_state_t : std::uint32_t {
        CONFIGURED = 1 << 0,
        LINKING = 1 << 1,
        LINKED = 1 << 2,
    };
    std::uint32_t plugin_state = 0;

    register_map_t register_map;
    discovery_map_t discovery_map;

    void link_registry() noexcept;
    void on_link(const std::error_code &ec) noexcept;
    bool has_registering() noexcept;
    virtual void continue_init(const std::error_code &ec) noexcept;
};

} // namespace rotor::plugin
