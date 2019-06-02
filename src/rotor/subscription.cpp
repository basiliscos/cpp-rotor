#include "rotor/subscription.h"

using namespace rotor;

void subscription_t::subscribe(handler_ptr_t handler) { map[handler->type_index].emplace_back(handler); }

subscription_t::list_t *subscription_t::get_recipients(const subscription_t::slot_t &slot) noexcept {
    auto it = map.find(slot);
    if (it != map.end()) {
        return &it->second;
    }
    return nullptr;
}

std::size_t subscription_t::unsubscribe(handler_ptr_t handler) {
    auto &list = map.at(handler->type_index);
    auto it = list.begin();
    while (it != list.end()) {
        if (*it == handler) {
            it = list.erase(it);
        } else {
            ++it;
        }
    }
    return map.size();
}
