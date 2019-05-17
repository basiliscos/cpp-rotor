#include "rotor.hpp"
#include <boost/asio.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace asio = boost::asio;

struct my_payload_t {
  std::string name;

  template <typename T> my_payload_t(T arg) : name{arg} {}
};

struct my_supervisor_t : public rotor::supervisor_t {
  my_supervisor_t(rotor::system_context_t &ctx) : rotor::supervisor_t{ctx} {}

  void on_initialize(
      rotor::message_t<rotor::payload::initialize_actor_t> &) override {
    std::cout << "on_initialize \n";
    subscribe<my_payload_t>(&my_supervisor_t::on_message);
    send<my_payload_t>(address, "hello 2 from init");
  }

  void on_message(rotor::message_t<my_payload_t> &msg) {
    std::cout << "on_message : " << msg.payload.name << "\n";
  }
};

int main(int argc, char **argv) {

  asio::io_context io_context{1};
  try {
    rotor::system_context_t system_context(io_context);
    auto supervisor = system_context.create_supervisor<my_supervisor_t>();
    auto addr_sup = supervisor->get_address();
    supervisor->enqueue<my_payload_t>(addr_sup, "hello");
    supervisor->start();
    io_context.run();
  } catch (const std::exception &ex) {
    std::cout << "exception : " << ex.what();
  }

  std::cout << "exiting...\n";
  return 0;
}
