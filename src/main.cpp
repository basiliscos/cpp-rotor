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

struct my_message_t {
  std::string name;

  template <typename T> my_message_t(T arg) : name{arg} {}
};

int main(int argc, char **argv) {

  asio::io_context io_context{1};
  try {
    rotor::system_context_t actor_context{io_context};
    rotor::supervisor_t supervisor(actor_context);
    auto addr_sup = supervisor.get_address();
    supervisor.enqueue<my_message_t>(addr_sup, "hello");
    io_context.run();

  } catch (const std::exception &ex) {
    std::cout << "exception : " << ex.what();
  }

  std::cout << "exiting...\n";
  return 0;
}
