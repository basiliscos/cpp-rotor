//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/messages.hpp"
#include "rotor/handler.h"
#include "rotor/supervisor.h"

namespace rotor::payload {

handler_call_t::~handler_call_t() {
    auto ptr = orig_message.get();
    if (ptr && ptr->next_route && ptr->use_count() == 1) {
        ptr->address = std::move(ptr->next_route);
        auto &sup = handler->actor_ptr->get_supervisor();
        sup.enqueue(std::move(orig_message));
    }
}

} // namespace rotor::payload
