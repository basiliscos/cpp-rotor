#include "catch.hpp"
#include "rotor.hpp"
#include <boost/asio.hpp>

namespace r = rotor;
namespace asio = boost::asio;

struct sample_sup_t : public r::supervisor_t {
  std::uint32_t init_invoked;
  r::address_ptr_t init_addr;

  std::uint32_t start_invoked;

  sample_sup_t(r::system_context_t &ctx,const r::supervisor_config_t &config_) : r::supervisor_t{ctx, config_} {
      init_invoked = 0;
      start_invoked = 0;
  }

  void on_initialize(r::message_t<r::payload::initialize_actor_t> &msg) noexcept override {
    r::supervisor_t::on_initialize(msg);
    ++init_invoked;
    init_addr = msg.payload.actor_address;
  }

  virtual void on_start(r::message_t<r::payload::start_supervisor_t> &) noexcept override {
      ++start_invoked;
  }

};

TEST_CASE("on_initialize && on_start", "[supervisor]") {
    asio::io_context io_context{1};
    r::supervisor_config_t conf;
    r::system_context_t system_context(io_context);

    auto sup = system_context.create_supervisor<sample_sup_t>(conf);
    sup->process();

    REQUIRE(sup->init_invoked == 1);
    REQUIRE(sup->init_addr == sup->get_address());

    sup->do_start();
    sup->process();
    REQUIRE(sup->init_invoked == 1);
    REQUIRE(sup->start_invoked == 1);
}

