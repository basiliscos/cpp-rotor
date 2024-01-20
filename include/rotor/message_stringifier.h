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

/** \struct message_stringifier_t
 *  \brief Abstract interface for making textual/string representation of a message
 *
 * As the concrete/final message type might not be known at compile time
 * the message stringifier tries to do it's best by guessing final message type.
 * (In other words, because the double dispatch visitor pattern is not available).
 *
 * That means that the message stringifier potentially has **LOW PERFORMANCE**
 * and should NOT be used, for example, for logging every message at least in
 * production code.
 *
 */
struct ROTOR_API message_stringifier_t {
    virtual ~message_stringifier_t() = default;

    /** \brief returns string representation of a message */
    virtual std::string stringify(const message_base_t &) const;

    /** \brief dumps string representation of a message to output stream */
    virtual void stringify_to(std::ostream &, const message_base_t &) const = 0;
};

/** \brief smart pointer for message stringifier */
using message_stringifier_ptr_t = std::unique_ptr<message_stringifier_t>;

}; // namespace rotor

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
