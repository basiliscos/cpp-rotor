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
/*
template <typename T> struct forwarder_traits {
    using args_t = void;
};
template <typename A, typename... Args> struct forwarder_traits<void
(A::*)(Args... args)> { using args_t = std::tuple<Args...>;
};
*/

/*
template <typename T> struct curry_arg_t {
    T it;
    template<typename O, typename F>
    void inline call(O& obj, F&& fn) const noexcept{
        (obj.*fn)(it);
    }
};
template <> struct curry_arg_t<void>{
    template<typename O, typename F>
    void inline call(O& obj, F&& fn) const noexcept{
        (obj.*fn)();
    }
};
*/
/*
template<typename...  Args> struct args_forwarder_t;
template<typename T> struct args_forwarder_t<T,
std::enable_if<std::is_same_v<T,void>>> {}; template<typename T> struct
args_forwarder_t<T> { static_assert (std::is_move_constructible_v<T>, "argument
should be move-constructible"); T item; args_forwarder_t(T&&
it):item{std::move(it)}{}
};

template<typename T, typename...  Args> struct args_forwarder_t<T, Args...>:
public {

};

template<typename... Args> struct args_forwarder_t< std::enable_if<sizeof...
(Args) == 0, void>> { template<typename O, typename F> void inline call(O& obj,
F&& fn) const noexcept{ (obj.*fn)();
    }
};
*/

template <typename Actor, typename Handler, typename ErrHandler> struct forwarder_t {
    using typed_actor_ptr_t = intrusive_ptr_t<Actor>;
    using typed_sup_t = supervisor_asio_t;

    forwarder_t(Actor &actor_, Handler &&handler_, ErrHandler &&err_handler_)
        : typed_actor{&actor_}, handler{std::move(handler_)}, err_handler{std::move(err_handler_)} {}

    template <typename... Args> void operator()(const boost::system::error_code &ec, Args... args) noexcept {

        auto &sup = static_cast<typed_sup_t &>(typed_actor->get_supevisor());
        auto &strand = sup.get_strand();
        if (ec) {
            asio::defer(strand, [actor = typed_actor, handler = std::move(err_handler), ec = ec]() {
                ((*actor).*handler)(ec);
                actor->get_supevisor().do_process();
            });
        } else {
            if constexpr (sizeof...(Args) == 0) {
                asio::defer(strand, [actor = typed_actor, handler = std::move(handler)]() {
                    ((*actor).*handler)();
                    actor->get_supevisor().do_process();
                });
            } else {
                asio::defer(strand,
                            [actor = typed_actor, handler = std::move(handler), args_fwd = curry_arg_t{args...}]() {
                                args_fwd.call(*actor, std::move(handler));
                                actor->get_supevisor().do_process();
                            });
            }
        }
    }

    typed_actor_ptr_t typed_actor;
    Handler handler;
    ErrHandler err_handler;
};

} // namespace asio

} // namespace rotor
