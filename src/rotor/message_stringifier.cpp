#include "rotor/message_stringifier.h"

#include <sstream>

using namespace rotor;

std::string message_stringifier_t::stringify(const message_base_t &message) const {
    std::stringstream stream;
    stringify_to(stream, message);
    return std::move(stream).str();
}
