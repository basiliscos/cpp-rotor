#include "rotor/message.h"

using type_map_t = std::unordered_map<std::string, const void *>;

namespace rotor::message_support {

const void *register_type(const std::type_index &type_index) noexcept {
    static type_map_t type_map = {};

    auto name = std::string(type_index.name());
    auto it = type_map.find(name);
    if (it != type_map.end()) {
        return it->second;
    }
    auto ptr = static_cast<const void *>(type_index.name());
    type_map[name] = ptr;
    return ptr;
}

} // namespace rotor::message_support
