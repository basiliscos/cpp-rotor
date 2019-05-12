#include <boost/asio.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace rotor {

namespace asio = boost::asio;

struct supervisor_t;

using counter_policy_t = boost::thread_unsafe_counter;

struct address_base_t
    : public boost::intrusive_ref_counter<address_base_t, counter_policy_t> {
  void *ctx_addr;
  address_base_t(void *ctx_addr_) : ctx_addr{ctx_addr_} {}
  bool operator==(const address_base_t &other) const { return this == &other; }
};
using address_ptr_t = boost::intrusive_ptr<address_base_t>;

struct actor_context_t {
public:
  actor_context_t(asio::io_context &io_context_) : io_context{io_context_} {}

  actor_context_t(const actor_context_t &) = delete;
  actor_context_t(actor_context_t &&) = delete;
  address_ptr_t make_address() {
    return address_ptr_t{new address_base_t(static_cast<void *>(this))};
  }

private:
  asio::io_context &io_context;
};

struct base_message_t
    : public boost::intrusive_ref_counter<base_message_t, counter_policy_t> {};

template <typename T> struct message_t : public base_message_t {

  template <typename... Args>
  message_t(Args... args) : payload{std::forward<Args>(args)...} {}

  T payload;
};

using message_ptr_t = boost::intrusive_ptr<base_message_t>;

struct supervisor_t {
public:
  supervisor_t(actor_context_t &actor_context_)
      : actor_context{actor_context_} {}

  template <typename M, typename... Args>
  void enqueue(const address_ptr_t &addr, Args &&... args) {
    auto raw_message = new message_t<M>(std::forward<Args>(args)...);
    outbound.emplace_back(addr, raw_message);
  }

private:
  struct item_t {
    address_ptr_t address;
    message_ptr_t message;

  public:
    item_t(const address_ptr_t &address_, message_ptr_t message_)
        : address{address_}, message{message_} {}
  };
  using queue_t = std::vector<item_t>;

  actor_context_t &actor_context;
  queue_t outbound;
};

} // namespace rotor

namespace asio = boost::asio;

struct my_message_t {
  std::string name;

  template <typename T> my_message_t(T arg) : name{arg} {}
};

int main(int argc, char **argv) {

  asio::io_context io_context{1};
  try {
    rotor::actor_context_t actor_context{io_context};
    rotor::supervisor_t supervisor(actor_context);
    // rotor::address_t addr_sup{supervisor};
    rotor::address_ptr_t addr_sup = actor_context.make_address();
    supervisor.enqueue<my_message_t>(addr_sup, "hello");
    io_context.run();

  } catch (const std::exception &ex) {
    std::cout << "exception : " << ex.what();
  }

  std::cout << "exiting...\n";
  return 0;
}
