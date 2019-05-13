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
struct system_context_t;
using counter_policy_t = boost::thread_unsafe_counter;

template <typename T>
using arc_base_t = boost::intrusive_ref_counter<T, counter_policy_t>;

struct address_t : public arc_base_t<address_t> {
  const void *ctx_addr;

private:
  friend class system_context_t;
  address_t(void *ctx_addr_) : ctx_addr{ctx_addr_} {}
  bool operator==(const address_t &other) const { return this == &other; }
};
using address_ptr_t = boost::intrusive_ptr<address_t>;

struct system_context_t {
public:
  system_context_t(asio::io_context &io_context_) : io_context{io_context_} {}

  system_context_t(const system_context_t &) = delete;
  system_context_t(system_context_t &&) = delete;

  address_ptr_t make_address() {
    return address_ptr_t{new address_t(static_cast<void *>(this))};
  }

private:
  asio::io_context &io_context;
};

struct message_base_t : public arc_base_t<message_base_t> {};

template <typename T> struct message_t : public message_base_t {
  template <typename... Args>
  message_t(Args... args) : payload{std::forward<Args>(args)...} {}

  T payload;
};
using message_ptr_t = boost::intrusive_ptr<message_base_t>;

struct actor_base_t {
  actor_base_t(system_context_t &system_context_)
      : system_context{system_context_}, address{
                                             system_context.make_address()} {}
  address_ptr_t get_address() const { return address; }

private:
  system_context_t &system_context;
  address_ptr_t address;
};

struct supervisor_t : public actor_base_t {
public:
  supervisor_t(system_context_t &system_context_)
      : actor_base_t{system_context_} {}

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
