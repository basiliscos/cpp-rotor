#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "handler.hpp"
#include "message.h"
#include <typeindex>
#include <map>
#include <vector>

namespace rotor {

/** \struct subscription_t
 *  \brief Holds and classifies message handlers on behalf of supervisor
 *
 * The handlers are classified by message type and by the source supervisor, i.e.
 * whether the hander's supervisor is external or not.
 *
 */
struct subscription_t {
    /** \struct classified_handlers_t
     *  \brief Holds @{link handler_base_t} and flag wheher it belongs to the
     * source supervisor
     */
    struct classified_handlers_t {
        /** \brief intrusive pointer to the handler */
        handler_ptr_t handler;
        /** \brief true if the hanlder is local */
        bool mine;
    };
    /** \brief list of classified handlers */
    using list_t = std::vector<classified_handlers_t>;

    /** \brief alias for message type */
    using slot_t = const void *;

    /** \brief constructor which takes the source @{link supervisor_t}  reference */
    subscription_t(supervisor_t &sup);

    /** \brief records the subscription for the handler */
    void subscribe(handler_ptr_t handler);

    /** \brief removes the records the subscription and returns amount of left subscriptions */
    std::size_t unsubscribe(handler_ptr_t handler);

    /** \brief optioally returns classified list of subscribers to the message type */
    list_t *get_recipients(const slot_t &slot) noexcept;

  private:
    using map_t = std::map<slot_t, list_t>;

    supervisor_t &supervisor;
    map_t map;
};

} // namespace rotor
