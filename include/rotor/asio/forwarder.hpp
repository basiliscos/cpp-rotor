#pragma once

//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/actor_base.h"
#include "rotor/asio/supervisor_asio.h"
#include <boost/asio.hpp>
#include <tuple>

namespace rotor::asio {

namespace {
namespace to {
struct supervisor {};
} // namespace to
} // namespace
} // namespace rotor::asio

namespace rotor {

/** \brief access to supervisor */
template <> inline auto &actor_base_t::access<asio::to::supervisor>() noexcept { return supervisor; }

namespace asio {

namespace asio = boost::asio;

namespace details {

namespace count {
using _0 = std::integral_constant<size_t, 0>;
using _1 = std::integral_constant<size_t, 1>;
} // namespace count

/** \brief asio callback decomposer into 1 possible argument type and total arguments count (0 or 1) */
template <typename T> struct callback_traits {};

/** \brief callback decomposer specialization for reference argument */
template <typename Class, typename M> struct callback_traits<void (Class::*)(M &) noexcept> {
    /** \brief number of arguments for asio callback (1) */
    using args_count = count::_1;

    /** \brief argument type is a reference */
    using arg_type = M &;
};

/** \brief callback decomposer specialization for value argument */
template <typename Class, typename M> struct callback_traits<void (Class::*)(M) noexcept> {
    /** \brief number of arguments for asio callback (1) */
    using args_count = count::_1;

    /** \brief argument type is a value */
    using arg_type = M;
};

/** \brief callback decomposer specialization for no-argument callback */
template <typename Class> struct callback_traits<void (Class::*)() noexcept> {
    /** \brief number of arguments for asio callback (0) */
    using args_count = count::_0;

    /** \brief argument type is missing (void) */
    using arg_type = void;
};

} // namespace details

/** \brief return `strand` of the boost::asio aware actor */
template <typename Actor> inline boost::asio::io_context::strand &get_strand(Actor &actor);

/** \brief templated forwarder base class */
template <typename Actor, typename Handler, typename ErrHandler> struct forwarder_base_t;

/** \brief partial specialization of forwarder base when there is no error handler */
template <typename Actor, typename Handler> struct forwarder_base_t<Actor, Handler, void> {
    /** \brief alias for intrusive pointer for an actor type */
    using typed_actor_ptr_t = intrusive_ptr_t<Actor>;

    /** \brief alias for boost::asio supervisor */
    using typed_sup_t = supervisor_asio_t;

    /** \brief constructs forwarder_base for the actor from it handler function */
    forwarder_base_t(Actor &actor_, Handler &&handler_) : typed_actor{&actor_}, handler{std::move(handler_)} {}

    /** intrusive pointer to the actor */
    typed_actor_ptr_t typed_actor;

    /** pointer-to-member function of the actor to handle positive case result */
    Handler handler;
};

/** \brief forwarder base implementation with result and error handlers */
template <typename Actor, typename Handler, typename ErrHandler>
struct forwarder_base_t : forwarder_base_t<Actor, Handler, void> {
    /** \brief alias for base class */
    using base_t = forwarder_base_t<Actor, Handler, void>;

    /** \brief constructs forwarder_base for the actor from it's member functions: positive
     * result handler and error handler functions.
     */
    forwarder_base_t(Actor &actor_, Handler &&handler_, ErrHandler &&err_handler_)
        : base_t{actor_, std::move(handler_)}, err_handler{std::move(err_handler_)} {}

    /** pointer-to-member function of the actor to handle negative case result, i.e. `error_code` */
    ErrHandler err_handler;
};

template <typename Actor, typename Handler, typename ArgsCount, typename ErrHandler> struct forwarder_t {};

/**
 * \brief dispatches single boost::asio callback with `error_code` and non-error results
 * into two different functions of the `actor`. After the invocation, actor's supervisor
 * `do_process` method is called to process message queue.
 *
 * The invocation is `strand`-aware.
 *
 */
template <typename Actor, typename Handler, typename ErrHandler>
struct forwarder_t<Actor, Handler, details::count::_0, ErrHandler> : forwarder_base_t<Actor, Handler, ErrHandler> {
    /** \brief alias for base class */
    using base_t = forwarder_base_t<Actor, Handler, ErrHandler>;
    using base_t::base_t;

    /** \brief mimics boost::asio handler, which will be forwarded/decomposed into
     * two different methods of the actor
     */
    template <typename T = void> inline void operator()(const boost::system::error_code &ec) noexcept {
        auto &typed_actor = base_t::typed_actor;
        auto &sup = static_cast<supervisor_asio_t &>(typed_actor->get_supervisor());
        auto &strand = get_strand(sup);
        if (ec) {
            asio::defer(strand, [actor = base_t::typed_actor, handler = std::move(base_t::err_handler), ec]() {
                ((*actor).*handler)(ec);
                actor->get_supervisor().do_process();
            });
        } else {
            asio::defer(strand, [actor = base_t::typed_actor, handler = std::move(base_t::handler)]() {
                ((*actor).*handler)();
                actor->get_supervisor().do_process();
            });
        }
    }
};

/** \brief forwarder specialization for 1-argument asio-callback */
template <typename Actor, typename Handler, typename ErrHandler>
struct forwarder_t<Actor, Handler, details::count::_1, ErrHandler> : forwarder_base_t<Actor, Handler, ErrHandler> {
    /** \brief alias for base class */
    using base_t = forwarder_base_t<Actor, Handler, ErrHandler>;
    using base_t::base_t;

    /** \brief mimics boost::asio handler, which will be forwarded/decomposed into
     * two different methods of the actor
     */
    template <typename T> inline void operator()(const boost::system::error_code &ec, T arg) noexcept {
        auto &typed_actor = base_t::typed_actor;
        auto &sup = static_cast<supervisor_asio_t &>(typed_actor->get_supervisor());
        auto &strand = get_strand(sup);
        if (ec) {
            asio::defer(strand, [actor = base_t::typed_actor, handler = std::move(base_t::err_handler), ec = ec]() {
                ((*actor).*handler)(ec);
                actor->get_supervisor().do_process();
            });
        } else {
            asio::defer(strand, [actor = base_t::typed_actor, handler = std::move(base_t::handler),
                                 arg = std::move(arg)]() mutable {
                ((*actor).*handler)(std::move(arg));
                actor->get_supervisor().do_process();
            });
        }
    }
};

/** \brief forwarder specialization for 1-argument asio-callback without error handler */
template <typename Actor, typename Handler>
struct forwarder_t<Actor, Handler, details::count::_0, void> : forwarder_base_t<Actor, Handler, void> {
    /** \brief alias for base class */
    using base_t = forwarder_base_t<Actor, Handler, void>;
    using base_t::base_t;

    /** \brief mimics boost::asio handler, which will be forwarded/decomposed into
     * two different methods of the actor
     */
    template <typename T = void> inline void operator()() noexcept {
        auto &typed_actor = base_t::typed_actor;
        auto &sup = static_cast<supervisor_asio_t &>(typed_actor->get_supervisor());
        auto &strand = get_strand(sup);
        asio::defer(strand, [actor = base_t::typed_actor, handler = std::move(base_t::handler)]() {
            ((*actor).*handler)();
            actor->get_supervisor().do_process();
        });
    }
};

/** \brief forwarder specialization, when error case is not possible  */
template <typename Actor, typename Handler>
struct forwarder_t<Actor, Handler, details::count::_1, void> : forwarder_base_t<Actor, Handler, void> {
    /** \brief alias for base class */
    using base_t = forwarder_base_t<Actor, Handler, void>;
    using base_t::base_t;

    /** \brief mimics boost::asio handler, which will be forwarded to the appropriate
     * actor's member method.
     */
    template <typename T> inline void operator()(T arg) noexcept {
        auto &typed_actor = base_t::typed_actor;
        auto &sup = static_cast<supervisor_asio_t &>(typed_actor->get_supervisor());
        auto &strand = get_strand(sup);
        asio::defer(strand, [actor = base_t::typed_actor, handler = std::move(base_t::handler),
                             arg = std::move(arg)]() mutable {
            ((*actor).*handler)(std::move(arg));
            actor->get_supervisor().do_process();
        });
    }
};

/** construtor deduction guide for forwarder (without error handler) */
template <typename Actor, typename Handler>
forwarder_t(Actor &, Handler &&)
    -> forwarder_t<Actor, Handler, typename details::callback_traits<Handler>::args_count, void>;

/** construtor deduction guide for forwarder (with error handler) */
template <typename Actor, typename Handler, typename ErrHandler>
forwarder_t(Actor &, Handler &&, ErrHandler &&)
    -> forwarder_t<Actor, Handler, typename details::callback_traits<Handler>::args_count, ErrHandler>;

} // namespace asio

} // namespace rotor
