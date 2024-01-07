//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include "supervisor_test.h"
#include "access.h"

namespace r = rotor;
namespace rt = r::test;

static const void *my_tag = &my_tag;

struct payload {
    const void *data;
};
using message_t = r::message_t<payload>;

struct sample_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;
    bool received = false;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([this](auto &p) {
            r::subscription_info_ptr_t info =
                p.subscribe_actor(r::lambda<message_t>([this](message_t &) noexcept { received = true; }));
            info->access<rt::to::tag, const void *>(my_tag);
        });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        send<payload>(address, my_tag);
    }
};

TEST_CASE("lambda handler + interceptor", "[actor]") {
    r::system_context_t system_context;

    bool intercepted = false;
    rt::interceptor_t interceptor = [&](auto &msg, const void *tag, const auto &cont) {
        if (tag == my_tag) {
            auto my_msg = static_cast<message_t *>(msg.get());
            if (my_msg->payload.data == my_tag) {
                intercepted = true;
            }
        }
        cont();
    };
    auto sup = system_context.create_supervisor<rt::supervisor_test_t>()
                   .interceptor(std::move(interceptor))
                   .timeout(rt::default_timeout)
                   .finish();
    auto actor = sup->create_actor<sample_actor_t>().timeout(rt::default_timeout).finish();
    sup->do_process();

    REQUIRE(sup->active_timers.size() == 0);
    REQUIRE(actor->received == true);

    sup->do_shutdown();
    sup->do_process();

    REQUIRE(sup->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup->get_leader_queue().size() == 0);
    REQUIRE(sup->get_points().size() == 0);
    REQUIRE(rt::empty(sup->get_subscription()));
    CHECK(intercepted);
}
