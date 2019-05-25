#pragma once

#include "handler.hpp"
#include "message.hpp"
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace rotor {

struct subscription_t {
  using list_t = std::vector<handler_ptr_t>;
  using slot_t = std::type_index;
  using map_t = std::unordered_map<slot_t, list_t>;

  void subscribe(handler_ptr_t handler) {
    map[handler->get_type_index()].emplace_back(handler);
  }

  list_t *get_recipients(const slot_t &slot) {
    auto it = map.find(slot);
    if (it != map.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void unsubscribe(handler_ptr_t handler) {
    auto &list = map.at(handler->get_type_index());
    auto it = list.begin();
    while (it != list.end()) {
      if (*it == handler) {
        it = list.erase(it);
      } else {
        ++it;
      }
    }
  }

private:
  map_t map;
};

} // namespace rotor
