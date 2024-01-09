#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "message.h"
#include "extended_error.h"
#include "forward.hpp"
#include <unordered_map>

namespace rotor {

/** \struct request_base_t
 *  \brief base class for request payload
 */
struct request_base_t {
    /** \brief unique (per supervisor) request id */
    request_id_t id;

    /** \brief destination address for reply
     *
     * It is not necessary that the address be the original actor
     *
     */
    address_ptr_t reply_to;

    /** \brief the source (original) actor address, which made an request */
    address_ptr_t origin;
};

/** \brief optionally wraps request type into intrusive pointer
 *
 * The transformation (`T -> iptr<T>`) occurs only if the type
 * `T` is descendant of  `arc_base_t<T>`.
 *
 * Otherwise, the user supplied request type `T` remains the same.
 *
 */
template <typename T, typename = void> struct request_wrapper_t {
    /** \brief an alias for the original request type */
    using request_t = T;
};

/** \brief request_wrapper_t specialization for refcounted item */
template <typename T> struct request_wrapper_t<T, std::enable_if_t<std::is_base_of_v<arc_base_t<T>, T>>> {
    /** \brief an alias for the original request type, wrapped into intrusive pointer */
    using request_t = intrusive_ptr_t<T>;
};

/** \brief optionally unwraps request type from intrusive pointer
 *
 * The transformation (`iptr<T> -> T`) occurs only if the type
 * `T` already wraps T into intrusive pointer
 *
 * Otherwise, the user supplied request type `T` remains the same.
 *
 */
template <typename T, typename = void> struct request_unwrapper_t {
    /** \brief an alias for the original request type */
    using request_t = T;
};

/** \brief request_unwrapper_t specialization for intrusive pointer of T */
template <typename T> struct request_unwrapper_t<intrusive_ptr_t<T>> {
    /** \brief an alias for the original request type, if it was wrapped into intrusive poitner */
    using request_t = T;
};

/** \struct wrapped_request_t
 * \brief templated request, which is able to hold user-supplied payload
 *
 * It contains as user-supplied payload as well as rotor-specific
 * information for request tracking (request_id, reply address).
 *
 */
template <typename T, typename = void> struct wrapped_request_t : request_base_t {
    /** \brief alias for original (unwrapped) request payload type */
    using request_t = T;

    /** \brief alias for original (unwrapped) response payload type */
    using response_t = typename T::response_t;

    /** \brief constructs wrapper for user-supplied payload from request-id and
     * and destination reply address */
    template <typename... Args>
    wrapped_request_t(request_id_t id_, const address_ptr_t &reply_to_, const address_ptr_t &origin_, Args &&...args)
        : request_base_t{id_, reply_to_, origin_}, request_payload{std::forward<Args>(args)...} {}

    /** \brief original, user-supplied payload */
    T request_payload;
};

/** \struct cancellation_t
 * \brief Request cancellation payload
 *
 */
template <typename T> struct cancellation_t {
    /** \brief unique (per supervisor) request id */
    request_id_t id;

    /** \brief actor address, which initiated the request */
    address_ptr_t source;
};

/**
 * \brief wrapped request specialization, when the request should be wrapped into intrusive pointer
 */
template <typename T>
struct wrapped_request_t<T, std::enable_if_t<std::is_base_of_v<arc_base_t<T>, T>>> : request_base_t {
    /** \brief alias for original (unwrapped) request payload type */
    using raw_request_t = T;

    /** \brief alias for original request payload type wrapped into intrusive pointer */
    using request_t = intrusive_ptr_t<T>;

    /** \brief alias for original (unwrapped) response payload type */
    using response_t = typename T::response_t;

    /** \brief makes an intrusive pointer for already constructed user-supplied payload
     *
     * The different `request-id` and `reply_to` address arguments are supplied
     * to make it possible cheaply forward requests.
     */
    wrapped_request_t(request_id_t id_, const address_ptr_t &reply_to_, const address_ptr_t &origin_,
                      const request_t &request_)
        : request_base_t{id_, reply_to_, origin_}, request_payload{request_} {}

    /** \brief constructs wrapper for user-supplied payload from request-id and
     * and destination reply address */
    template <typename... Args, typename E = std::enable_if_t<std::is_constructible_v<raw_request_t, Args...>>>
    wrapped_request_t(request_id_t id_, const address_ptr_t &reply_to_, const address_ptr_t &origin_, Args &&...args)
        : request_base_t{id_, reply_to_, origin_}, request_payload{new raw_request_t{std::forward<Args>(args)...}} {}

    /** \brief intrusive pointer to user-supplied payload */
    request_t request_payload;
};

/** \struct response_helper_t
 * \brief generic helper, which helps to construct user-defined response payload
 */
template <typename Response> struct response_helper_t {
    /** original user-supplied response type */
    using response_t = Response;

    /** \brief constructs user defined response payload */
    template <typename... Args> static Response construct(Args &&...args) {
        return Response{std::forward<Args>(args)...};
    }
};

/**
 * \brief specific helper, which helps to construct intrusive pointer to user-defined
 *  response payload
 */
template <typename Response> struct response_helper_t<intrusive_ptr_t<Response>> {
    /** \brief type for intrusive pointer user defined response payload  */
    using res_ptr_t = intrusive_ptr_t<Response>;

    /** original user-supplied response type */
    using response_t = Response;

    /** \brief constructs intrusive pointer to user defined response payload */
    template <typename... Args> static res_ptr_t construct(Args &&...args) {
        return res_ptr_t{new Response{std::forward<Args>(args)...}};
    }

    /** \brief constructs user defined response payload and wraps it into intrusive pointer */
    template <typename T, typename std::enable_if_t<std::is_same_v<T, res_ptr_t>>> static res_ptr_t construct(T &&ptr) {
        return res_ptr_t{std::forward<T>(ptr)};
    }
};

namespace details {

template <class T, typename... Args> decltype(void(T{std::declval<Args>()...}), std::true_type()) test(int);

template <class T, typename... Args> std::false_type test(...);

template <class T, typename... Args> struct is_braces_constructible : decltype(test<T, Args...>(0)) {};

template <class T, class... Args> constexpr auto is_braces_constructible_v = is_braces_constructible<T, Args...>::value;

template <typename... Ts> struct size_of_t;
template <typename T> struct size_of_t<T> : std::is_default_constructible<T> {};
template <typename T, typename... Ts> struct size_of_t<T, Ts...> : public std::is_constructible<T, Ts...> {};

template <typename T, typename... Args>
inline constexpr bool is_somehow_constructible_v =
    std::is_constructible_v<T, Args...> || is_braces_constructible_v<T, Args...>;

/** \brief main helper to check constructability of T from Args... */
template <typename T, typename E = void, typename... Args>
struct is_constructible : is_constructible<T, void, E, Args...> {};

/** \brief checks whether T is default-constructible  */
template <typename T> struct is_constructible<T, void> {
    /** \brief returns true fi type T is default-constructible */
    static constexpr auto value = std::is_default_constructible_v<T>;
};

/** \brief checks whether it is possible construct T from Arg */
template <typename T, typename Arg> struct is_constructible<T, Arg> {
    /** \brief returns true fi type T is constructible or braces-constructible from `Arg` */
    static constexpr auto value = is_somehow_constructible_v<T, Arg>;
};

/** \brief checks whether it is possible construct T from Arg */
template <typename T, typename Arg> struct is_constructible<T, void, Arg> {
    /** \brief returns true fi type T is constructible or braces-constructible from `Arg` */
    static constexpr auto value = is_somehow_constructible_v<T, Arg>;
};

/** \brief checks whether it is possible construct T from Args... */
template <typename T, typename... Args> struct is_constructible<T, void, Args...> {
    /** \brief returns true fi type T is constructible or braces-constructible from `Args...` */
    static constexpr auto value = is_somehow_constructible_v<T, Args...>;
};

template <typename T, typename... Args> inline constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

} // namespace details

/** \struct wrapped_response_t
 * \brief trackable templated response which holds user-supplied response payload.
 *
 * In addition to user-supplied response payload, the class contains `error_code`
 * and intrusive pointer to the original request message.
 *
 */
template <typename Request> struct wrapped_response_t {
    /** \brief alias for original user-supplied request type */
    using request_t = typename request_unwrapper_t<Request>::request_t;

    /** \brief alias type of message with wrapped request, which is possibly wrapped into intrusive pointer */
    using req_message_t = message_t<wrapped_request_t<request_t>>;

    /** \brief alias for intrusive pointer to message with wrapped request */
    using req_message_ptr_t = intrusive_ptr_t<req_message_t>;

    /** \brief alias for possibly wrapped user-supplied response type */
    using response_t = typename request_t::response_t;

    /** \brief helper type for response construction */
    using res_helper_t = response_helper_t<response_t>;

    /** \brief alias user-supplied response type */
    using unwrapped_response_t = typename res_helper_t::response_t;

    static_assert(std::is_default_constructible_v<response_t>, "response type must be default-constructible");

    /** \brief pointer to extended error, used in the case of response failure */
    extended_error_ptr_t ee;

    /** \brief original request message, which contains request_id for request/response matching */
    req_message_ptr_t req;

    /** \brief user-supplied response payload */
    response_t res;

    /** \brief error-response constructor (response payload is empty) */
    wrapped_response_t(const extended_error_ptr_t &ee_, req_message_ptr_t message_)
        : ee{ee_}, req{std::move(message_)} {}

    /** \brief "forward-constructor"
     *
     * The request message, error code are copied, while the response (possible intrusive
     * pointer to the original request) is forwarded.
     *
     */
    template <typename Response, typename E = std::enable_if_t<std::is_same_v<response_t, std::remove_cv_t<Response>>>>
    wrapped_response_t(req_message_ptr_t message_, const extended_error_ptr_t &ee_, Response &&res_)
        : ee{ee_}, req{std::move(message_)}, res{std::forward<Response>(res_)} {}

    /** \brief successful-response constructor.
     *
     * The request message is copied, the error code is set to success,
     * the response (possible intrusive pointer to the original request) constructed from
     * the arguments.
     *
     */
    template <typename Req, typename... Args,
              typename E1 = std::enable_if_t<std::is_same_v<req_message_ptr_t, std::remove_cv_t<Req>>>,
              typename E2 = std::enable_if_t<details::is_constructible_v<unwrapped_response_t, Args...>>>
    wrapped_response_t(Req &&message_, Args &&...args)
        : ee{}, req{std::forward<Req>(message_)}, res{res_helper_t::construct(std::forward<Args>(args)...)} {}

    /** \brief returns request id of the original request */
    inline request_id_t request_id() const noexcept { return req->payload.id; }
};

/** \brief free function type, which produces error response to the original request */
typedef message_ptr_t(error_producer_t)(const address_ptr_t &reply_to, message_base_t &msg,
                                        const extended_error_ptr_t &ec) noexcept;

/** \struct request_curry_t
 * \brief the recorded context, which is needed to produce error response to the original request */
struct request_curry_t {
    /** \brief the free function, which produces error response */
    error_producer_t *fn;

    /** \brief destination address for the error response */
    address_ptr_t origin;

    /** \brief the original request message */
    message_ptr_t request_message;

    /** \brief actor, on which behalf the original request has been made */
    actor_base_t *source;
};

/** \struct request_traits_t
 * \brief type helper to deduce request/response messages from original (user-supplied) request type
 */
template <typename R> struct request_traits_t {
    /** \brief alias for original request payload type */
    using request_t = typename request_unwrapper_t<R>::request_t;

    /** \struct request
     * \brief request related types */
    struct request {
        /** \brief alias for possibly wrapped (to intrusive pointer) request payload type */
        using type = request_t;

        /** \brief wrapped (trackable) request payload */
        using wrapped_t = wrapped_request_t<type>;

        /** \brief message type with with wrapped request payload */
        using message_t = rotor::message_t<wrapped_t>;

        /** \brief intrusive pointer type for request message */
        using message_ptr_t = intrusive_ptr_t<message_t>;

        using visitor_t = typename message_t::visitor_t;
    };

    /** \struct response
     * \brief response related types */
    struct response {

        /** \brief wrapped response payload (contains original request message) */
        using wrapped_t = wrapped_response_t<request_t>;

        /** \brief message type for wrapped response */
        using message_t = rotor::message_t<wrapped_t>;

        /** \brief intrusive pointer type for response message */
        using message_ptr_t = intrusive_ptr_t<message_t>;

        using visitor_t = typename message_t::visitor_t;
    };

    /** \struct cancel
     * \brief cancel request related types */
    struct cancel {

        /** \brief the payload needed to cancel a request */
        using cancel_payload_t = cancellation_t<request_t>;

        /** \brief request cancellation message */
        using message_t = rotor::message_t<cancel_payload_t>;

        using visitor_t = typename message_t::visitor_t;
    };

    /** \brief helper free function to produce error reply to the original request */
    static message_ptr_t make_error_response(const address_ptr_t &reply_to, message_base_t &message,
                                             const extended_error_ptr_t &ee) noexcept {
        using reply_message_t = typename response::message_t;
        using request_message_ptr = typename request::message_ptr_t;
        auto &request = static_cast<typename request::message_t &>(message);
        auto req_ptr = request_message_ptr(&request);
        auto raw_reply = new reply_message_t{reply_to, ee, req_ptr};
        return message_ptr_t{raw_reply};
    }
};

/** \struct request_builder_t
 * \brief builder pattern implementation for the original request
 */
template <typename T> struct [[nodiscard]] request_builder_t {

    /** \brief constructs request message but still does not dispatch it */
    template <typename... Args>
    request_builder_t(supervisor_t &sup_, actor_base_t &actor_, const address_ptr_t &destination_,
                      const address_ptr_t &reply_to_, Args &&...args);

    /** \brief actually dispatches requests and spawns timeout timer
     *
     * The request id of the dispatched request is returned
     *
     */
    request_id_t send(const pt::time_duration &send) noexcept;

  private:
    using traits_t = request_traits_t<T>;
    using request_message_t = typename traits_t::request::message_t;
    using request_message_ptr_t = typename traits_t::request::message_ptr_t;
    using response_message_t = typename traits_t::response::message_t;
    using response_message_ptr_t = typename traits_t::response::message_ptr_t;
    using wrapped_res_t = typename traits_t::response::wrapped_t;

    supervisor_t &sup;
    actor_base_t &actor;
    request_id_t request_id;
    const address_ptr_t &destination;
    const address_ptr_t &reply_to;
    bool do_install_handler;
    request_message_ptr_t req;
    address_ptr_t imaginary_address;

    void install_handler() noexcept;
};

} // namespace rotor
