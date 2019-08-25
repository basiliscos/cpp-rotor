#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "message.h"
#include "messages.hpp"
#include "error_code.h"

namespace rotor {

struct request_base_t {
    std::uint32_t id;
    actor_ptr_t reply_to;
};

template <typename T> struct wrapped_request_t : request_base_t { T payload; };

template <typename T> using request_ptr_t = intrusive_ptr_t<wrapped_request_t<T>>;

template <typename T> using responce_ptr_t = intrusive_ptr_t<T>;

struct responce_base_t {
    virtual inline ~responce_base_t();
    virtual const request_base_t &get_request() noexcept = 0;
};

template <typename Responce, typename Request> struct wrapped_responce_t : public responce_base_t {
    using request_t = request_ptr_t<Request>;
    using responce_t = responce_ptr_t<Responce>;

    error_code_t ec;
    request_t req;
    responce_t res;

    wrapped_responce_t(error_code_t ec_, const request_t &req_) : ec{ec_}, req{req_} {}

    wrapped_responce_t(const request_t &req_, const responce_t &res_)
        : ec{error_code_t::success}, req{req_}, res{res_} {}

    virtual const request_base_t &get_request() noexcept override { return *req; }
};

} // namespace rotor
