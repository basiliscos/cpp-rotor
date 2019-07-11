#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "arc.hpp"
#include "address.hpp"
#include <typeindex>

namespace rotor {

struct message_base_t : public arc_base_t<message_base_t> {
    virtual ~message_base_t();
    virtual const void *get_type_index() const noexcept = 0;

    message_base_t(const address_ptr_t &addr) : address{addr} {}

    address_ptr_t address;
};

inline message_base_t::~message_base_t() {}

template <typename T> struct message_t : public message_base_t {
    using payload_t = T;
    template <typename... Args>
    message_t(const address_ptr_t &addr, Args... args) : message_base_t{addr}, payload{std::forward<Args>(args)...} {}

    virtual const void *get_type_index() const noexcept { return message_type; }

    T payload;
    static const void *message_type;
};
using message_ptr_t = intrusive_ptr_t<message_base_t>;

template <typename T> const void *message_t<T>::message_type = static_cast<const void *>(typeid(message_t<T>).name());

} // namespace rotor
