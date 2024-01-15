#pragma once

//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/export.h"

#include <string>
#include <sstream>
#include <memory>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace rotor {

struct message_base_t;

struct ROTOR_API message_stringifier_t {
    virtual ~message_stringifier_t() = default;
    virtual std::string stringify(const message_base_t &) const;
    virtual void stringify_to(std::stringstream &, const message_base_t &) const = 0;
};

using message_stringifier_ptr_t = std::unique_ptr<message_stringifier_t>;

}; // namespace rotor

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
