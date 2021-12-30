#pragma once

//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "actor_base.h"
#include "message.h"
#include "forward.hpp"
#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <type_traits>

namespace rotor {

/** \struct lambda_holder_t
 *
 * \brief Helper struct which holds lambda function for processing particular message types
 *
 * The whole purpose of the structure is to allow to deduce the lambda argument, i.e.
 * message type.
 *
 */
template <typename M, typename F> struct lambda_holder_t {
    /** \brief lambda function itself */
    F fn;

    /** \brief constructs lambda by forwarding arguments */
    explicit lambda_holder_t(F &&fn_) : fn(std::forward<F>(fn_)) {}

    /** \brief alias type for message type for lambda */
    using message_t = M;

    /** \brief alias type for message payload */
    using payload_t = typename M::payload_t;
};

/** \brief helper function for lambda holder constructing */
template <typename M, typename F> constexpr lambda_holder_t<M, F> lambda(F &&fn) {
    return lambda_holder_t<M, F>(std::forward<F>(fn));
}

/** \struct handler_traits
 *  \brief Helper class to extract final actor class and message type from pointer-to-member function
 */
template <typename T> struct handler_traits {};

/** \struct handler_traits<void (A::*)(M &) >
 *  \brief Helper class to catch wrong message handler
 */
template <typename A, typename M> struct handler_traits<void (A::*)(M &)> {
    using pointer_t = void (A::*)(M &);
    static auto const constexpr has_noexcept =
        noexcept((std::declval<A>().*std::declval<pointer_t>())(std::declval<M &>()));
    static_assert(has_noexcept, "message handler should have 'noexcept' specification");
};

/** \struct handler_traits<void (A::*)(M &) noexcept>
 *  \brief Helper class to extract final actor class and message type from pointer-to-member function
 */
template <typename A, typename M> struct handler_traits<void (A::*)(M &) noexcept> {

    /** \brief returns true if message is valid */
    static auto const constexpr has_valid_message = std::is_base_of_v<message_base_t, M>;

    /** \brief returns true if handler belongs to actor */
    static auto const constexpr is_actor = std::is_base_of_v<actor_base_t, A>;

    /** \brief returns true if handler belongs to plugin */
    static auto const constexpr is_plugin = std::is_base_of_v<plugin::plugin_base_t, A>;

    /** \brief returns true if it is lambda-handler */
    static auto const constexpr is_lambda = false;

    /** \brief message type, processed by the handler */
    using message_t = M;

    /** \brief alias for message type payload */
    using payload_t = typename M::payload_t;

    /** \brief alias for Actor or Plugin class */
    using backend_t = A;
    static_assert(is_actor || is_plugin, "message handler should be either actor or plugin");
};

/** \brief handler decomposer for lambda-based handler */
template <typename M, typename H> struct handler_traits<lambda_holder_t<M, H>> {
    /** \brief returns true if message is valid */
    static auto const constexpr has_valid_message = std::is_base_of_v<message_base_t, M>;

    static_assert(has_valid_message, "lambda does not process valid message");

    /** \brief not an actor handler */
    static auto const constexpr is_actor = false;

    /** \brief not a plugin handler */
    static auto const constexpr is_plugin = false;

    /** \brief yes, it is lambda handler */
    static auto const constexpr is_lambda = true;
};

/** \struct handler_base_t
 *  \brief Base class for `rotor` handler, i.e concrete message type processing point
 * on concrete actor.
 *
 * It holds reference to {@link actor_base_t}.
 */
struct handler_base_t : public arc_base_t<handler_base_t> {
    /** \brief pointer to unique message type ( `typeid(Message).name()` ) */
    const void *message_type;

    /** \brief pointer to unique handler type ( `typeid(Handler).name()` ) */
    const void *handler_type;

    /** \brief non-null pointer to {@link actor_base_t} the actor of the handler,  */
    actor_base_t* actor_ptr;

    /** \brief precalculated hash for the handler */
    size_t precalc_hash;

    /** \brief constructs `handler_base_t` from raw pointer to actor, raw
     * pointer to message type and raw pointer to handler type
     */
    explicit handler_base_t(actor_base_t &actor, const void *message_type_, const void *handler_type_) noexcept;

    /** \brief compare two handler for equality */
    inline bool operator==(const handler_base_t &rhs) const noexcept {
        return handler_type == rhs.handler_type && actor_ptr == rhs.actor_ptr;
    }

    /** \brief attempt to delivery message to the handler
     *
     * The message is delivered only if its type matches to the handler message type,
     * otherwise it is silently ignored
     */
    virtual void call(message_ptr_t &) noexcept = 0;

    /** \brief "upgrades" handler by tagging it
     *
     * Conceptually it intercepts handler call and does tag-specific actions
     *
     */
    virtual handler_ptr_t upgrade(const void *tag) noexcept;

    virtual ~handler_base_t();

    /** \brief returns `true` if the message can be handled by the handler */
    virtual bool select(message_ptr_t &) noexcept = 0;

    /** \brief unconditionlally invokes the handler for the message
     *
     * It assumes that the handler is able to handle the message. See
     * `select` method.
     *
     */
    virtual void call_no_check(message_ptr_t &) noexcept = 0;
};

/** \struct continuation_t
 * \brief continue handler invocation (used for intercepting)
 */
struct continuation_t {

    /** \brief continue handler invocation */
    virtual void operator()() const noexcept = 0;
};

/** \struct handler_intercepted_t
 * \brief proxies call to the original hanlder, applying tag-specific actions
 */
struct handler_intercepted_t : public handler_base_t {
    /** \brief constructs `handler_intercepted_t` by proxying original hander */
    handler_intercepted_t(handler_ptr_t backend_, const void *tag_) noexcept;

    handler_ptr_t upgrade(const void *tag) noexcept override;
    void call(message_ptr_t &) noexcept override;
    bool select(message_ptr_t &message) noexcept override;
    void call_no_check(message_ptr_t &message) noexcept override;

  private:
    handler_ptr_t backend;
    const void *tag;
};

namespace details {

template <typename Handler>
inline constexpr bool is_actor_handler_v =
    handler_traits<Handler>::is_actor && !handler_traits<Handler>::is_plugin &&
    handler_traits<Handler>::has_valid_message && !handler_traits<Handler>::is_lambda;

template <typename Handler> inline constexpr bool is_lambda_handler_v = handler_traits<Handler>::is_lambda;

template <typename Handler>
inline constexpr bool is_plugin_handler_v =
    handler_traits<Handler>::has_valid_message &&handler_traits<Handler>::is_plugin &&
    !handler_traits<Handler>::is_actor && !handler_traits<Handler>::is_lambda;

namespace {
namespace to {
struct actor {};
} // namespace to
} // namespace
} // namespace details

/** \brief access to actor via plugin */
template <> inline auto &plugin::plugin_base_t::access<details::to::actor>() noexcept { return actor; }

template <typename Handler, typename Enable = void> struct handler_t;

/**
 *  \brief the actor handler meant to hold user-specific pointer-to-member function
 *  \tparam Handler pointer-to-member function type
 */
template <typename Handler>
struct handler_t<Handler, std::enable_if_t<details::is_actor_handler_v<Handler>>> : public handler_base_t {

    /** \brief static pointer to unique pointer-to-member function name ( `typeid(Handler).name()` ) */
    static const void *handler_type;

    /** \brief pointer-to-member function instance */
    Handler handler;

    /** \brief constructs handler from actor & pointer-to-member function  */
    explicit handler_t(actor_base_t &actor, Handler &&handler_)
        : handler_base_t{actor, final_message_t::message_type, handler_type}, handler{handler_} {}

    void call(message_ptr_t &message) noexcept override {
        if (message->type_index == final_message_t::message_type) {
            auto final_message = static_cast<final_message_t *>(message.get());
            auto &final_obj = static_cast<backend_t &>(*actor_ptr);
            (final_obj.*handler)(*final_message);
        }
    }

    bool select(message_ptr_t &message) noexcept override {
        return message->type_index == final_message_t::message_type;
    }

    void call_no_check(message_ptr_t &message) noexcept override {
        auto final_message = static_cast<final_message_t *>(message.get());
        auto &final_obj = static_cast<backend_t &>(*actor_ptr);
        (final_obj.*handler)(*final_message);
    }

  private:
    using traits = handler_traits<Handler>;
    using backend_t = typename traits::backend_t;
    using final_message_t = typename traits::message_t;
};

template <typename Handler>
const void *handler_t<Handler, std::enable_if_t<details::is_actor_handler_v<Handler>>>::handler_type =
    static_cast<const void *>(typeid(Handler).name());

/**
 * \brief handler specialization for plugin
 */
template <typename Handler>
struct handler_t<Handler, std::enable_if_t<details::is_plugin_handler_v<Handler>>> : public handler_base_t {
    /** \brief typeid of Handler */
    static const void *handler_type;

    /** \brief source plugin for handler */
    plugin::plugin_base_t &plugin;

    /** \brief handler itself */
    Handler handler;

    /** \brief ctor form plugin and plugin handler (pointer-to-member function of the plugin) */
    explicit handler_t(plugin::plugin_base_t &plugin_, Handler &&handler_)
        : handler_base_t{*plugin_.access<details::to::actor>(), final_message_t::message_type, handler_type},
          plugin{plugin_}, handler{handler_} {}

    void call(message_ptr_t &message) noexcept override {
        if (message->type_index == final_message_t::message_type) {
            auto final_message = static_cast<final_message_t *>(message.get());
            auto &final_obj = static_cast<backend_t &>(plugin);
            (final_obj.*handler)(*final_message);
        }
    }

    bool select(message_ptr_t &message) noexcept override {
        return message->type_index == final_message_t::message_type;
    }

    void call_no_check(message_ptr_t &message) noexcept override {
        auto final_message = static_cast<final_message_t *>(message.get());
        auto &final_obj = static_cast<backend_t &>(plugin);
        (final_obj.*handler)(*final_message);
    }

  private:
    using traits = handler_traits<Handler>;
    using backend_t = typename traits::backend_t;
    using final_message_t = typename traits::message_t;
};

template <typename Handler>
const void *handler_t<Handler, std::enable_if_t<details::is_plugin_handler_v<Handler>>>::handler_type =
    static_cast<const void *>(typeid(Handler).name());

/**
 * \brief handler specialization for lambda handler
 */
template <typename Handler, typename M>
struct handler_t<lambda_holder_t<Handler, M>,
                 std::enable_if_t<details::is_lambda_handler_v<lambda_holder_t<Handler, M>>>> : public handler_base_t {
    /** \brief alias type for lambda, which will actually process messages */
    using handler_backend_t = lambda_holder_t<Handler, M>;

    /** \brief actuall lambda function for message processing */
    handler_backend_t handler;

    /** \brief static pointer to unique pointer-to-member function name ( `typeid(Handler).name()` ) */
    static const void *handler_type;

    /** \brief constructs handler from actor & lambda wrapper */
    explicit handler_t(actor_base_t &actor, handler_backend_t &&handler_)
        : handler_base_t{actor, final_message_t::message_type, handler_type}, handler{std::forward<handler_backend_t>(
                                                                                  handler_)} {}

    void call(message_ptr_t &message) noexcept override {
        if (message->type_index == final_message_t::message_type) {
            auto final_message = static_cast<final_message_t *>(message.get());
            handler.fn(*final_message);
        }
    }

    bool select(message_ptr_t &message) noexcept override {
        return message->type_index == final_message_t::message_type;
    }

    void call_no_check(message_ptr_t &message) noexcept override {
        auto final_message = static_cast<final_message_t *>(message.get());
        handler.fn(*final_message);
    }

  private:
    using final_message_t = typename handler_backend_t::message_t;
};

template <typename Handler, typename M>
const void *handler_t<lambda_holder_t<Handler, M>,
                      std::enable_if_t<details::is_lambda_handler_v<lambda_holder_t<Handler, M>>>>::handler_type =
    static_cast<const void *>(typeid(Handler).name());

} // namespace rotor

namespace std {
/** \struct hash<rotor::handler_ptr_t>
 *  \brief Hash calculator for handler
 */
template <> struct hash<rotor::handler_ptr_t> {

    /** \brief Returns the precalculated hash for the hanlder */
    size_t operator()(const rotor::handler_ptr_t &handler) const noexcept { return handler->precalc_hash; }
};
} // namespace std
