# Rationale

[sobjectizer]: https://github.com/Stiffstream/sobjectizer
[so_5_extra]: https://stiffstream.com/en/products/so_5_extra.html
[so_5_infra]: https://stiffstream.com/en/docs/sobjectizer/so_5-5/namespaceso__5_1_1env__infrastructures.html
[so_5_loops]: https://github.com/eao197/so-5-5/issues/25
[caf]: https://actor-framework.org/
[boost-asio]: https://www.boost.org/doc/libs/release/libs/asio/


There are two main actor frameworks in C++ world: [sobjectizer] and [c++ actor framework][caf].
There is major a issue with them, as they offer some actor runtime, which has own rules for
execution, which user have to comply. On the other hand there are a lot of third-party libraries,
which offten have other runtime. As the result, the seamless usage of them together becomes hard
or even impossible, or it might come at rather hight costs (i.e. running each environment
/runtime  on it's own thread). In the other words they are **intrusive**.

Their intrusiveness makes the frameworks samewhat unfriendly for cooperation with other C++
libraries. However, why they are intrusive? To my opinion, this happens because the frameworks
need, first, *timers* and, second, some inter-thread message passing and actor execution
environment. So, for the frameworks own intrusive runtime was developed.

`rotor` was designed with the opposite approach: let timeouts and inter-thread communications
be handled by external libraries (loops), while `rotor` responsibiliy is just internal message
delivery (and a few build-in rules for initialization/termiation of actors etc.). As the result
`rotor` is quite lightweigth micro-framework; the main infrastructure units of execution (aka
`supevisors`) are tightly coupled to the concrete event loop. However the last point shouldn't
be a big issue, as soon as the `rotor` *messaging interface* does not depend on underlying
loop. In `rotor` messages can be passed between actors running on different threads or even
actors running on different event loops, as *soon as underlying supervisors/loops support
that*.

As the last point, it should be noted, that [sobjectizer] is not that intrusive as [caf],
because it supports various [infrastructures][so_5_infra] and because there is an additional
external module [so_5_extra], which already has [boost-asio] support. Alas, it has different
licence, not so commerce-friendly as [sobjectizer]. The external loop integration is still
[non-trivial task][so_5_loops] compared to `rotor`, and I'm still not sure
whether is is possible to integrate more than one event loop.

If you are familiar with [sobjectizer], the `supervisor` concept in `rotor` plays the similar
role as `cooperation` in [sobjectizer].
