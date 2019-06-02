#pragma once

#include "handler.hpp"
#include "message.h"
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace rotor {

struct subscription_t {
    struct classified_handlers_t {
        bool mine;
        handler_ptr_t handler;
    };
    using list_t = std::vector<classified_handlers_t>;
    using slot_t = const void *;
    using map_t = std::unordered_map<slot_t, list_t>;

    subscription_t(supervisor_t &sup);

    void subscribe(handler_ptr_t handler);
    std::size_t unsubscribe(handler_ptr_t handler);
    list_t *get_recipients(const slot_t &slot) noexcept;

  private:
    supervisor_t &supervisor;
    map_t map;
};

} // namespace rotor
