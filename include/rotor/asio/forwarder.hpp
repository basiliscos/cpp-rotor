#pragma once

#include "rotor/actor_base.h"
#include "rotor/asio/supervisor_asio.h"
#include <boost/asio.hpp>
#include <tuple>

namespace rotor {

namespace asio {

namespace asio = boost::asio;

template <typename... Args> struct curry_arg_t {
    std::tuple<Args...> args;

    template <typename O, typename F> void inline call(O &obj, F &&fn) const noexcept {
        using seq_t = std::make_index_sequence<sizeof...(Args)>;
        (obj.*fn)(std::get<seq_t>(args));
    }
};

template <typename Actor> inline boost::asio::io_context::strand &get_strand(Actor &actor);

template <typename Actor, typename Handler, typename ErrHandler = void> struct forwarder_t;
template <typename Actor, typename Handler, typename ErrHandler> struct forwarder_t {
    using typed_actor_ptr_t = intrusive_ptr_t<Actor>;
    using typed_sup_t = supervisor_asio_t;

    forwarder_t(Actor &actor_, Handler &&handler_, ErrHandler &&err_handler_)
        : typed_actor{&actor_}, handler{std::move(handler_)}, err_handler{std::move(err_handler_)} {}

    template <typename... Args> void operator()(const boost::system::error_code &ec, Args... args) noexcept {
        auto &sup = static_cast<typed_sup_t &>(typed_actor->get_supervisor());
        auto &strand = get_strand(sup);
        if (ec) {
            asio::defer(strand, [actor = typed_actor, handler = std::move(err_handler), ec = ec]() {
                ((*actor).*handler)(ec);
                actor->get_supervisor().do_process();
            });
        } else {
            if constexpr (sizeof...(Args) == 0) {
                asio::defer(strand, [actor = typed_actor, handler = std::move(handler)]() {
                    ((*actor).*handler)();
                    actor->get_supervisor().do_process();
                });
            } else {
                asio::defer(strand,
                            [actor = typed_actor, handler = std::move(handler), args_fwd = curry_arg_t{args...}]() {
                                args_fwd.call(*actor, std::move(handler));
                                actor->get_supervisor().do_process();
                            });
            }
        }
    }

    typed_actor_ptr_t typed_actor;
    Handler handler;
    ErrHandler err_handler;
};

template <typename Actor, typename Handler> struct forwarder_t<Actor, Handler, void> {
    using typed_actor_ptr_t = intrusive_ptr_t<Actor>;
    using typed_sup_t = supervisor_asio_t;

    forwarder_t(Actor &actor_, Handler &&handler_) : typed_actor{&actor_}, handler{std::move(handler_)} {}

    template <typename... Args> void operator()(Args... args) noexcept {
        auto &sup = static_cast<typed_sup_t &>(typed_actor->get_supervisor());
        auto &strand = get_strand(sup);

        if constexpr (sizeof...(Args) == 0) {
            asio::defer(strand, [actor = typed_actor, handler = std::move(handler)]() {
                ((*actor).*handler)();
                actor->get_supervisor().do_process();
            });
        } else {
            asio::defer(strand, [actor = typed_actor, handler = std::move(handler), args_fwd = curry_arg_t{args...}]() {
                args_fwd.call(*actor, std::move(handler));
                actor->get_supervisor().do_process();
            });
        }
    }
    typed_actor_ptr_t typed_actor;
    Handler handler;
};

template <typename Actor, typename Handler>
forwarder_t(Actor &actor_, Handler &&handler_)->forwarder_t<Actor, Handler, void>;

} // namespace asio

} // namespace rotor
