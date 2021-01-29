#pragma once

//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"
#include <string>
#include <system_error>

namespace rotor {

struct extended_error_t;

/** \brief intrusive pointer to exteneded error type */
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
struct extended_error_t : arc_base_t<extended_error_t> {
    std::string context;
    std::error_code ec;
    extended_error_ptr_t next;

    extended_error_t(const std::string &context_, const std::error_code &ec_,
                     const extended_error_ptr_t &next_ = {}) noexcept
        : context{context_}, ec{ec_}, next{next_} {}

    /** \brief human-readeable detailed description of the error
     *
     * First, it stringifies own error in accordance with the context.
     *
     * Second, it recursively ask details on all following errors, appedning them
     * into the result. The result string is returned.
     */
    std::string message() const noexcept;
};

extended_error_ptr_t make_error(const std::string &context_, const std::error_code &ec_,
                                const extended_error_ptr_t &next_ = {}) noexcept;

} // namespace rotor
