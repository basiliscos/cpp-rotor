#pragma once

//
// Copyright (c) 2021-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"
#include "message.h"
#include <string>
#include <system_error>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace rotor {

struct extended_error_t;
struct message_stringifier_t;

/** \brief intrusive pointer to extended error type */
using extended_error_ptr_t = intrusive_ptr_t<extended_error_t>;

/** \struct extended_error_t
 *  \brief Holds string context, error_code and the pointer to the following error.
 *
 *
 * This is extension over std::error_code, to make it possible to identify the
 * context of the error (usually it is actor identity), and make it possible to
 * construct the chain of failures, that's why there is a smart pointer to the
 * next error.
 *
 */
struct ROTOR_API extended_error_t : arc_base_t<extended_error_t> {
    /** \brief error context, usually actor identity */
    std::string context;

    /** \brief abstract error code, describing occurred error */
    std::error_code ec;

    /** \brief pointer to the parent error */
    extended_error_ptr_t next;

    /** \brief pointer request caused error */
    message_ptr_t request;

    /** \brief extened error constructor */
    extended_error_t(const std::string &context_, const std::error_code &ec_, const extended_error_ptr_t &next_ = {},
                     const message_ptr_t &request_ = {}) noexcept
        : context{context_}, ec{ec_}, next{next_}, request{request_} {}

    /** \brief human-readable detailed description of the error
     *
     * First, it stringifies own error in accordance with the context.
     *
     * Second, it recursively ask details on all following errors, appending them
     * into the result. The result string is returned.
     */
    std::string message(const message_stringifier_t *stringifier = nullptr) const noexcept;

    /**
     * \brief returns root (inner-most) extended error
     */
    extended_error_ptr_t root() const noexcept;
};

/** \brief constructs smart pointer to the extened error */
ROTOR_API extended_error_ptr_t make_error(const std::string &context_, const std::error_code &ec_,
                                          const extended_error_ptr_t &next_ = {},
                                          const message_ptr_t &request_ = {}) noexcept;

} // namespace rotor

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
