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

/** \struct request_base_t
 *  \brief base class for request payload
 */
struct request_base_t {
    /** \brief unique (per supervisor) request id */
    std::uint32_t id;

    /** \brief destination addrress for reply
     *
     * It is not neccessary that the address be the original actor
     *
     */
    address_ptr_t reply_to;
};

/** \struct wrapped_request_t
 * \brief templated request, which is able to hold user-supplied payload
 *
 * It contains as user-supplied payload as well as rotor-specific
 * information for request tracking (request_id, reply address).
 *
 */
template <typename T> struct wrapped_request_t : request_base_t {
    /** \brief alias for original (unwrapped) request payload type */
    using request_t = T;

    /** \brief alias for original (unwrapped) response payload type */
    using response_t = typename T::response_t;

    /** \brief constructs wrapper for user-supplied payload from request-id and
     * and destination reply address */
    template <typename... Args>
    wrapped_request_t(std::uint32_t id_, const address_ptr_t &reply_to_, Args &&... args)
        : request_base_t{id_, reply_to_}, request_payload{std::forward<Args>(args)...} {}

    /** \brief original, user-supplied payload */
    T request_payload;
};

/** \struct response_helper_t
 * \brief generic helper, which helps to construct user-defined response payload
 */
template <typename Responce> struct response_helper_t {
    /** \brief constructs user defined response payload */
    template <typename... Args> static Responce construct(Args &&... args) {
        return Responce{std::forward<Args>(args)...};
    }
};

/** \struct response_helper_t<intrusive_ptr_t<Responce>>
 * \brief specific helper, which helps to construct intrusive pointer to user-defined
 *  response payload
 */
template <typename Responce> struct response_helper_t<intrusive_ptr_t<Responce>> {
    /** \brief type for intrusive pointer user defined response payload  */
    using res_ptr_t = intrusive_ptr_t<Responce>;

    /** \brief constructs intrusive pointer to user defined response payload */
    template <typename... Args> static res_ptr_t construct(Args &&... args) {
        return res_ptr_t{new Responce{std::forward<Args>(args)...}};
    }
};

/** \struct wrapped_response_t
 * \brief trackable templated response which holds user-supplied response payload.
 *
 * In addition to user-supplied response payload, the class contains `error_code`
 * and intrusive pointer to the original request message.
 *
 */
template <typename Request> struct wrapped_response_t {
    /** \brief alias type of message with wrapped request */
    using req_message_t = message_t<wrapped_request_t<Request>>;

    /** \brief alias for intrusive pointer to message with wrapped request */
    using req_message_ptr_t = intrusive_ptr_t<req_message_t>;

    /** \brief alias for original user-supplied request type */
    using request_t = Request;

    /** \brief alias for original user-supplied response type */
    using response_t = typename Request::response_t;

    /** \brief helper type for response construction */
    using res_helper_t = response_helper_t<response_t>;

    static_assert(std::is_default_constructible_v<response_t>, "response type must be default-constructible");

    /** \brief error code of processing request, i.e. `error_code_t::request_timeout` */
    std::error_code ec;

    /** \brief original request message, which contains request_id for request/response matching */
    req_message_ptr_t req;

    /** \brief user-supplied response payload */
    response_t res;

    /** \brief error-response constructor (response payload is empty) */
    wrapped_response_t(std::error_code ec_, req_message_ptr_t message_) : ec{ec_}, req{std::move(message_)} {}

    /** \brief success-response constructor */
    template <typename... Args>
    wrapped_response_t(req_message_ptr_t message_, Args &&... args)
        : ec{make_error_code(error_code_t::success)}, req{std::move(message_)}, res{res_helper_t::construct(
                                                                                    std::forward<Args>(args)...)} {}
    /** \brief returns request id of the original request */
    inline std::int32_t request_id() const noexcept { return req->payload.id; }
};

/** \brief free function type, which produces error response to the original request */
typedef message_ptr_t(error_producer_t)(const address_ptr_t &reply_to, message_base_t &msg,
                                        const std::error_code &ec) noexcept;

/** \struct request_curry_t
 * \brief the recorded context, which is needed to produce error response to the original request */
struct request_curry_t {
    /** \brief the free function, which produces error response */
    error_producer_t *fn;

    /** \brief destination address for the error response */
    address_ptr_t reply_to;

    /** \brief the original request message */
    message_ptr_t request_message;
};

/** \struct request_traits_t
 * \brief type helper to deduce request/reqsponce messages from original (user-supplied) request type
 */
template <typename R> struct request_traits_t {

    /** \struct request
     * \brief request related types */
    struct request {
        /** \brief alias for original request payload type */
        using type = R;

        /** \brief wrapped (trackeable) request payload */
        using wrapped_t = wrapped_request_t<R>;

        /** \brief message type with with wrapped request payload */
        using message_t = rotor::message_t<wrapped_t>;

        /** \brief intrusive pointer type for request message */
        using message_ptr_t = intrusive_ptr_t<message_t>;
    };

    /** \struct response
     * \brief response related types */
    struct response {

        /** \brief wrapped response payload (contains original request message) */
        using wrapped_t = wrapped_response_t<R>;

        /** \brief message type for wrapped response */
        using message_t = rotor::message_t<wrapped_t>;

        /** \brief intrusive pointer type for response message */
        using message_ptr_t = intrusive_ptr_t<message_t>;
    };

    /** \brief helper free function to produce error reply to the original request */
    static message_ptr_t make_error_response(const address_ptr_t &reply_to, message_base_t &message,
                                             const std::error_code &ec) noexcept {
        using reply_message_t = typename response::message_t;
        using request_message_ptr = typename request::message_ptr_t;
        auto &request = static_cast<typename request::message_t &>(message);
        auto req_ptr = request_message_ptr(&request);
        auto raw_reply = new reply_message_t{reply_to, ec, req_ptr};
        return message_ptr_t{raw_reply};
    }
};

/** \struct request_builder_t
 * \brief builder pattern implentation for the original request
 */
template <typename T> struct [[nodiscard]] request_builder_t {

    /** \brief constructs request message but still does not dispath it */
    template <typename... Args>
    request_builder_t(supervisor_t & sup_, actor_base_t & actor_, const address_ptr_t &destination_,
                      const address_ptr_t &reply_to_, Args &&... args);

    /** \brief actually dispatches requests and spawns timeout timer
     *
     * The request id of the dispatched request is returned
     *
     */
    std::uint32_t send(pt::time_duration send) noexcept;

  private:
    using traits_t = request_traits_t<T>;
    using request_message_t = typename traits_t::request::message_t;
    using request_message_ptr_t = typename traits_t::request::message_ptr_t;
    using response_message_t = typename traits_t::response::message_t;
    using response_message_ptr_t = typename traits_t::response::message_ptr_t;
    using wrapped_res_t = typename traits_t::response::wrapped_t;

    supervisor_t &sup;
    actor_base_t &actor;
    std::uint32_t request_id;
    const address_ptr_t &destination;
    const address_ptr_t &reply_to;
    bool do_install_handler;
    request_message_ptr_t req;
    address_ptr_t imaginary_address;

    void install_handler() noexcept;
};

} // namespace rotor
