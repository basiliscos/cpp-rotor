#pragma once

#include "handler.hpp"
#include "message.h"
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace rotor {

struct subscription_t {
    using list_t = std::vector<handler_ptr_t>;
    using slot_t = std::type_index;
    using map_t = std::unordered_map<slot_t, list_t>;

    void subscribe(handler_ptr_t handler);
    std::size_t unsubscribe(handler_ptr_t handler);
    list_t *get_recipients(const slot_t &slot);

  private:
    map_t map;
};

} // namespace rotor
