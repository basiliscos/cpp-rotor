#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "message.h"
#include "error_code.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <memory>

namespace rotor {

namespace pt = boost::posix_time;

struct request_base_t : public arc_base_t<request_base_t> {
    std::uint32_t id;
    address_ptr_t reply_to;
    request_base_t(std::uint32_t id_, const address_ptr_t &reply_to_) : id{id_}, reply_to{reply_to_} {}
    virtual ~request_base_t();
};

template <typename T> struct wrapped_request_t : request_base_t {
    using request_t = T;
    using responce_t = typename T::responce_t;

    template <typename... Args>
    wrapped_request_t(std::uint32_t id_, const address_ptr_t &reply_to_, Args &&... args)
        : request_base_t{id_, reply_to_}, payload{std::forward<Args>(args)...} {}

    T payload;
};

struct responce_base_t : public arc_base_t<request_base_t> {
    virtual ~responce_base_t();
    virtual const request_base_t &get_request() noexcept = 0;
};

template <typename Request> struct wrapped_responce_t : public responce_base_t {
    using request_ptr_t = intrusive_ptr_t<wrapped_request_t<Request>>;
    using responce_t = typename Request::responce_t;
    using responce_ptr_t = typename std::unique_ptr<responce_t>;

    error_code_t ec;
    request_ptr_t req;
    responce_ptr_t res;

    wrapped_responce_t(error_code_t ec_, const request_ptr_t &req_) : ec{ec_}, req{req_} {}

    wrapped_responce_t(const request_ptr_t &req_, responce_ptr_t &&res_)
        : ec{error_code_t::success}, req{req_}, res{std::move(res_)} {}

    virtual const request_base_t &get_request() noexcept override { return *req; }
};

template <typename R> struct request_traits_t {
    using request_t = R;
    using responce_t = typename R::responce_t;
    using wrapped_req_t = wrapped_request_t<R>;
    using request_ptr_t = intrusive_ptr_t<wrapped_req_t>;
    using wrapped_res_t = wrapped_responce_t<R>;
    using wrapped_res_ptr_t = std::unique_ptr<wrapped_res_t>;
    using responce_ptr_t = std::unique_ptr<responce_t>;
    using request_message_t = message_t<request_ptr_t>;
    using responce_message_t = message_t<wrapped_res_t>;
    using responce_message_ptr_t = intrusive_ptr_t<responce_message_t>;
    using request_message_ptr_t = intrusive_ptr_t<request_message_t>;
};

template <typename T> struct [[nodiscard]] request_builder_t {
    using traits_t = request_traits_t<T>;
    using responce_t = typename traits_t::responce_t;
    using request_ptr_t = typename traits_t::request_ptr_t;
    using wrapped_request_t = typename traits_t::wrapped_req_t;
    using wrapped_res_t = typename traits_t::wrapped_res_t;
    using request_message_t = typename traits_t::request_message_t;
    using responce_message_t = typename traits_t::responce_message_t;

    supervisor_t &sup;
    std::uint32_t request_id;
    const address_ptr_t &destination;
    const address_ptr_t &reply_to;
    bool do_install_handler;
    request_ptr_t req;
    address_ptr_t imaginary_address;

    template <typename... Args>
    request_builder_t(supervisor_t & sup_, const address_ptr_t &destination_, const address_ptr_t &reply_to_,
                      Args &&... args);

    void timeout(pt::time_duration timeout) noexcept;

  private:
    void install_handler() noexcept;
};

} // namespace rotor
