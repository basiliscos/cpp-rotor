//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/message.h"
#include <unordered_map>

using type_map_t = std::unordered_map<std::string_view, const void *>;

namespace rotor::message_support {

const void *register_type(const std::type_index &type_index) noexcept {
    static type_map_t type_map = {};

    auto name = std::string_view(type_index.name());
    auto it = type_map.find(name);
    if (it != type_map.end()) {
        return it->second;
    }
    auto ptr = static_cast<const void *>(type_index.name());
    type_map[name] = ptr;
    return ptr;
}

} // namespace rotor::message_support
