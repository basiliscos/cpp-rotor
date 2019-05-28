#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"

namespace r = rotor;

struct sample_sup_t : public r::test::supervisor_test_t {
  std::uint32_t initialized;
  std::uint32_t init_invoked;
  std::uint32_t start_invoked;
  r::address_ptr_t init_addr;

  sample_sup_t() : r::test::supervisor_test_t{nullptr} {
      initialized = 0;
      init_invoked = 0;
      start_invoked = 0;
  }

  void do_initialize() noexcept override {
      ++initialized;
      r::test::supervisor_test_t::do_initialize();
  }

  void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
    r::supervisor_t::on_initialize(msg);
    ++init_invoked;
    init_addr = msg.payload.actor_address;
  }

  virtual void on_start(r::message_t<r::payload::start_supervisor_t> &) noexcept override {
      ++start_invoked;
  }

  void start() noexcept override {}
  void shutdown() noexcept override {}

};

TEST_CASE("on_initialize, on_start, on_shutdown", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<sample_sup_t>();
    REQUIRE(&sup->get_supevisor() == sup.get());
    REQUIRE(sup->initialized == 1);

    sup->process();
    REQUIRE(sup->init_invoked == 1);
    REQUIRE(sup->init_addr == sup->get_address());

    sup->do_start();
    sup->process();
    REQUIRE(sup->init_invoked == 1);
    REQUIRE(sup->start_invoked == 1);
}

