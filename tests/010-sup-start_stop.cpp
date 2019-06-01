#include "catch.hpp"
#include "rotor.hpp"
#include "supervisor_test.h"

namespace r = rotor;
namespace rt = rotor::test;

struct sample_sup_t : public rt::supervisor_test_t {
  using sup_base_t = rt::supervisor_test_t;
  std::uint32_t initialized;
  std::uint32_t init_invoked;
  std::uint32_t start_invoked;
  std::uint32_t shutdown_req_invoked;
  std::uint32_t shutdown_conf_invoked;
  r::address_ptr_t init_addr;
  r::actor_ptr_t shutdown_actor;

  sample_sup_t() : r::test::supervisor_test_t{nullptr} {
      initialized = 0;
      init_invoked = 0;
      start_invoked = 0;
      shutdown_req_invoked = 0;
      shutdown_conf_invoked = 0;
  }

  void do_initialize() noexcept override {
      ++initialized;
      sup_base_t::do_initialize();
  }

  void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
    sup_base_t::on_initialize(msg);
    ++init_invoked;
    init_addr = msg.payload.actor_address;
  }

  virtual void on_start(r::message_t<r::payload::start_actor_t> &msg) noexcept override {
      ++start_invoked;
      sup_base_t::on_start(msg);
  }

  virtual void on_shutdown(r::message_t<r::payload::shutdown_request_t> &msg) noexcept override {
      ++shutdown_req_invoked;
      sup_base_t::on_shutdown(msg);
  }

  virtual void on_shutdown_confirm(r::message_t<r::payload::shutdown_confirmation_t> &msg) noexcept override {
      ++shutdown_conf_invoked;
      shutdown_actor = msg.payload.actor;
      sup_base_t::on_shutdown_confirm(msg);
  }

};

TEST_CASE("on_initialize, on_start, simple on_shutdown", "[supervisor]") {
    r::system_context_t system_context;

    auto sup = system_context.create_supervisor<sample_sup_t>();
    REQUIRE(&sup->get_supevisor() == sup.get());
    REQUIRE(sup->initialized == 1);

    sup->do_process();
    REQUIRE(sup->init_invoked == 1);
    REQUIRE(sup->init_addr == sup->get_address());

    sup->do_start();
    sup->do_process();
    REQUIRE(sup->init_invoked == 1);
    REQUIRE(sup->start_invoked == 1);
    REQUIRE(sup->shutdown_req_invoked == 0);
    REQUIRE(sup->shutdown_conf_invoked == 0);

    sup->do_shutdown();
    sup->do_process();
    REQUIRE(sup->shutdown_req_invoked == 1);
    REQUIRE(sup->shutdown_conf_invoked == 1);
    REQUIRE(dynamic_cast<sample_sup_t*>(sup->shutdown_actor.get()) == sup.get());
}

