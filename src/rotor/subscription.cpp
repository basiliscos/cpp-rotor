#include "rotor/subscription.h"

using namespace rotor;

void subscription_t::subscribe(handler_ptr_t handler) {
  map[handler->get_type_index()].emplace_back(handler);
}

subscription_t::list_t *
subscription_t::get_recipients(const subscription_t::slot_t &slot) {
  auto it = map.find(slot);
  if (it != map.end()) {
    return &it->second;
  }
  return nullptr;
}

void subscription_t::unsubscribe(handler_ptr_t handler) {
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
