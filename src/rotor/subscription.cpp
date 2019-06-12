#include "rotor/subscription.h"
#include "rotor/supervisor.h"

using namespace rotor;

subscription_t::subscription_t(supervisor_t &sup) : supervisor{sup} {}

void subscription_t::subscribe(handler_ptr_t handler) {
    bool mine = &handler->raw_actor_ptr->get_supevisor() == &supervisor;
    map[handler->type_index].emplace_back(classified_handlers_t{mine, std::move(handler)});
}

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
        if (*it->handler == *handler) {
            it = list.erase(it);
        } else {
            ++it;
        }
    }
    if (list.empty()) {
        map.erase(handler->type_index);
    }
    return map.size();
}
