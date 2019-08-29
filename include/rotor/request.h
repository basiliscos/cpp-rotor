#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "message.h"
#include "error_code.h"
#include <unordered_map>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace rotor {

namespace pt = boost::posix_time;

struct request_base_t {
    std::uint32_t id;
    address_ptr_t reply_to;
};

template <typename T> struct wrapped_request_t : request_base_t {
    using request_t = T;
    using responce_t = typename T::responce_t;

    template <typename... Args>
    wrapped_request_t(std::uint32_t id_, const address_ptr_t &reply_to_, Args &&... args)
        : request_base_t{id_, reply_to_}, request_payload{std::forward<Args>(args)...} {}

    T request_payload;
};

template <typename Responce> struct responce_helper_t {
    template <typename... Args> static Responce construct(Args &&... args) {
        return Responce{std::forward<Args>(args)...};
    }
};

template <typename Responce> struct responce_helper_t<intrusive_ptr_t<Responce>> {
    using res_ptr_t = intrusive_ptr_t<Responce>;
    template <typename... Args> static res_ptr_t construct(Args &&... args) {
        return res_ptr_t{new Responce{std::forward<Args>(args)...}};
    }
};

template <typename Request> struct wrapped_responce_t {
    using req_message_t = message_t<wrapped_request_t<Request>>;
    using req_message_ptr_t = intrusive_ptr_t<req_message_t>;
    using request_t = Request;
    using responce_t = typename Request::responce_t;
    using res_helper_t = responce_helper_t<responce_t>;
    static_assert(std::is_default_constructible_v<responce_t>, "responce type must be default-constructible");

    std::error_code ec;
    req_message_ptr_t req;
    responce_t res;

    wrapped_responce_t(std::error_code ec_, req_message_ptr_t message_) : ec{ec_}, req{std::move(message_)} {}

    template <typename... Args>
    wrapped_responce_t(req_message_ptr_t message_, Args &&... args)
        : ec{make_error_code(error_code_t::success)}, req{std::move(message_)}, res{res_helper_t::construct(
                                                                                    std::forward<Args>(args)...)} {}

    inline std::int32_t request_id() const noexcept { return req->payload.id; }
};

template <typename R> struct request_traits_t {
    struct request {
        using type = R;
        using wrapped_t = wrapped_request_t<R>;
        using message_t = rotor::message_t<wrapped_t>;
        using message_ptr_t = intrusive_ptr_t<message_t>;
    };

    struct responce {
        using wrapped_t = wrapped_responce_t<R>;
        using message_t = rotor::message_t<wrapped_t>;
        using message_ptr_t = intrusive_ptr_t<message_t>;
    };
};

template <typename T> struct [[nodiscard]] request_builder_t {
    using traits_t = request_traits_t<T>;
    using request_message_t = typename traits_t::request::message_t;
    using request_message_ptr_t = typename traits_t::request::message_ptr_t;
    using responce_message_t = typename traits_t::responce::message_t;
    using responce_message_ptr_t = typename traits_t::responce::message_ptr_t;
    using wrapped_res_t = typename traits_t::responce::wrapped_t;

    supervisor_t &sup;
    std::uint32_t request_id;
    const address_ptr_t &destination;
    const address_ptr_t &reply_to;
    bool do_install_handler;
    request_message_ptr_t req;
    address_ptr_t imaginary_address;

    template <typename... Args>
    request_builder_t(supervisor_t & sup_, const address_ptr_t &destination_, const address_ptr_t &reply_to_,
                      Args &&... args);

    void timeout(pt::time_duration timeout) noexcept;

  private:
    void install_handler() noexcept;
};

struct request_subscription_t {

    struct key_t {
        const void *message_type;
        address_ptr_t source_addr;
        inline bool operator==(const key_t &other) const noexcept {
            return (source_addr.get() == other.source_addr.get()) && (message_type == other.message_type);
        }
    };

    struct hasher_t {
        inline size_t operator()(const key_t &k) const noexcept {
            return reinterpret_cast<size_t>(k.message_type) ^ reinterpret_cast<size_t>(k.source_addr.get());
        }
    };

    using map_t = std::unordered_map<key_t, address_ptr_t, hasher_t>;

    address_ptr_t get(const void *message_type, const address_ptr_t &source_addr) const noexcept;
    void set(const void *message_type, const address_ptr_t &source_addr, const address_ptr_t &dest_addr) noexcept;

    map_t map;
};

} // namespace rotor
