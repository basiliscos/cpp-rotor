//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "catch.hpp"
#include "rotor.hpp"
#include "actor_test.h"
#include "system_context_test.h"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = rotor::test;

constexpr std::uint32_t PID_1 = 1u << 1;
constexpr std::uint32_t PID_2 = 1u << 2;

struct sample_plugin1_t;
struct buggy_plugin_t;
struct sample_plugin2_t;

struct sample_actor_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;
    using plugins_list_t = std::tuple<sample_plugin1_t, sample_plugin2_t>;

    std::uint32_t init_seq = 0;
    std::uint32_t deinit_seq = 0;
};

struct sample_actor2_t : public sample_actor_t {
    using sample_actor_t::sample_actor_t;
    using plugins_list_t = std::tuple<sample_plugin1_t, sample_plugin2_t, buggy_plugin_t>;
};

struct sample_plugin1_t : public r::plugin_t {
    const void *identity() const noexcept override {
        return static_cast<const void *>(typeid(sample_plugin1_t).name());
    }

    void activate(r::actor_base_t *actor_) noexcept override {
        auto &init_seq = static_cast<sample_actor_t *>(actor_)->init_seq;
        init_seq = (init_seq << 8 | PID_1);
        return r::plugin_t::activate(actor_);
    }
    void deactivate() noexcept override {
        auto &deinit_seq = static_cast<sample_actor_t *>(actor)->deinit_seq;
        deinit_seq = (deinit_seq << 8 | PID_1);
        return r::plugin_t::deactivate();
    }
};

struct sample_plugin2_t : public r::plugin_t {
    const void *identity() const noexcept override {
        return static_cast<const void *>(typeid(sample_plugin2_t).name());
    }
    void activate(r::actor_base_t *actor_) noexcept override {
        auto &init_seq = static_cast<sample_actor_t *>(actor_)->init_seq;
        init_seq = (init_seq << 8 | PID_2);
        return r::plugin_t::activate(actor_);
    }
    void deactivate() noexcept override {
        auto &deinit_seq = static_cast<sample_actor_t *>(actor)->deinit_seq;
        deinit_seq = (deinit_seq << 8 | PID_2);
        return r::plugin_t::deactivate();
    }
};

struct buggy_plugin_t : public r::plugin_t {
    const void *identity() const noexcept override { return static_cast<const void *>(typeid(buggy_plugin_t).name()); }
    void activate(r::actor_base_t *actor_) noexcept override {
        actor = actor_;
        actor_->commit_plugin_activation(*this, false);
    }
};

TEST_CASE("init/deinit sequence", "[plugin]") {
    rt::system_context_test_t system_context;
    using builder_t = typename sample_actor_t::template config_builder_t<sample_actor_t>;
    auto builder = builder_t([&](auto &) {}, system_context);
    auto actor = std::move(builder).timeout(rt::default_timeout).finish();

    REQUIRE(actor->get_activating_plugins().size() == 2);
    REQUIRE(actor->get_deactivating_plugins().size() == 0);

    actor->activate_plugins();
    REQUIRE((actor->init_seq == ((PID_1 << 8) | PID_2)));

    REQUIRE(actor->get_activating_plugins().size() == 0);
    REQUIRE(actor->get_deactivating_plugins().size() == 0);

    actor->deactivate_plugins();
    REQUIRE((actor->deinit_seq == ((PID_2 << 8) | PID_1)));
    REQUIRE(actor->get_deactivating_plugins().size() == 0);
};

TEST_CASE("fail init-plugin sequence", "[plugin]") {
    rt::system_context_test_t system_context;
    using builder_t = typename sample_actor2_t::template config_builder_t<sample_actor2_t>;
    auto builder = builder_t([&](auto &) {}, system_context);
    auto actor = std::move(builder).timeout(rt::default_timeout).finish();

    REQUIRE(actor->get_activating_plugins().size() == 3);
    REQUIRE(actor->get_deactivating_plugins().size() == 0);

    actor->activate_plugins();
    CHECK(actor->init_seq == ((PID_1 << 8) | PID_2));

    REQUIRE(actor->get_activating_plugins().size() == 1);
    REQUIRE(actor->get_deactivating_plugins().size() == 0);

    CHECK(actor->deinit_seq == ((PID_2 << 8) | PID_1));
};
