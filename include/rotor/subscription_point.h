#pragma once

//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/forward.hpp"
#include "rotor/address.hpp"
#include <vector>
#include <list>

namespace rotor {

struct actor_base_t;

namespace tags {

extern const void *io;

}

/** \brief who owns the subscription point
 *
 * - plugin (i.e. subscription was invoked from plugin)
 *
 * - supervisor (temporally instantiated subscription for request/response handling)
 *
 * - foreign (when it is subscribed for foreign address)
 *
 * - anonymous (subscription point was created from actor)
 *
 */
enum class owner_tag_t {
    ANONYMOUS,
    PLUGIN,
    SUPERVISOR,
    FOREIGN,
};

/** \struct subscription_point_t
 *  \brief pair of {@link handler_base_t} linked to particular {@link address_t}
 */
struct subscription_point_t {
    /** \brief intrusive pointer to messages' handler */
    handler_ptr_t handler;

    /** \brief intrusive pointer to address */
    address_ptr_t address;

    /** \brief non-owning pointer to an actor, which performs subscription */
    const actor_base_t *owner_ptr;

    /** \brief kind of ownership of the subscription point */
    owner_tag_t owner_tag;

    /** \brief ctor from handler and related address (used for comparison) */
    subscription_point_t(const handler_ptr_t &handler_, const address_ptr_t &address_) noexcept
        : handler{handler_}, address{address_}, owner_ptr{nullptr} {}

    /** \brief full ctor (handler, address, actor, and owner tag) */
    subscription_point_t(const handler_ptr_t &handler_, const address_ptr_t &address_, const actor_base_t *owner_ptr_,
                         owner_tag_t owner_tag_) noexcept
        : handler{handler_}, address{address_}, owner_ptr{owner_ptr_}, owner_tag{owner_tag_} {}

    /** \brief copy-ctor */
    subscription_point_t(const subscription_point_t &) = default;

    /** \brief move-ctor */
    subscription_point_t(subscription_point_t &&) = default;

    /** \brief patrial comparison by handler and address */
    bool operator==(const subscription_point_t &other) const noexcept;
};

/** \struct subscription_info_t
 *  \brief {@link subscription_point_t} with extended information (e.g. state)
 */
struct subscription_info_t : public arc_base_t<subscription_info_t>, subscription_point_t {
    /** \brief subscription info state (subscribing, established, unsubscribing */
    enum state_t { SUBSCRIBING, ESTABLISHED, UNSUBSCRIBING };

    /** \brief ctor from subscription point, internal address and internal handler and state */
    subscription_info_t(const subscription_point_t &point, bool internal_address_, bool internal_handler_,
                        state_t state_) noexcept
        : subscription_point_t{point}, internal_address{internal_address_},
          internal_handler{internal_handler_}, state{state_} {}
    ~subscription_info_t();

    /** \brief uses {@link subscription_point_t} comparison */
    inline bool operator==(const subscription_point_t &point) const noexcept {
        return (subscription_point_t &)(*this) == point;
    }

    /** \brief marks handler for blocking operations
     *
     * It is suitable for thread backend.
     *
     */
    inline void tag_io() noexcept { tag(tags::io); }

    /** \brief generic non-public fields accessor */
    template <typename T> auto &access() noexcept;

    /** \brief generic non-public methods accessor */
    template <typename T, typename... Args> auto access(Args... args) noexcept;

  private:
    void tag(const void *t) noexcept;

    /** \brief whether the subscription point (info) belongs to internal address, i.e. to the "my" supervisor */
    bool internal_address;

    /** \brief whether the subscription point (info) has internal handler, i.e. will be processed by "my" supervisor */
    bool internal_handler;

    /** \brief subscription state */
    state_t state;
};

/** \brief intrusive pointer for {@link subscription_info_t} */
using subscription_info_ptr_t = intrusive_ptr_t<subscription_info_t>;

/** \struct subscription_container_t
 *  \brief list of {@link subscription_info_ptr_t} with possibility to find via {@link subscription_point_t}
 */
struct subscription_container_t : public std::list<subscription_info_ptr_t> {
    /** \brief looks up for the subscription info pointer (returned as iterator) via the subscription point */
    iterator find(const subscription_point_t &point) noexcept;
};

} // namespace rotor
