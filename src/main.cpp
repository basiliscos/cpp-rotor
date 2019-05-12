#include <boost/asio.hpp>
#include <iostream>
#include <type_traits>
#include <utility>

template <typename T, typename M, typename Marker> struct forwarder_t {
  forwarder_t(T &obj_, M &&member_, Marker &&marker_)
      : obj{obj_}, impl_fn{std::forward<M>(member_)}, marker{
                                                          std::forward<Marker>(
                                                              marker_)} {}

  template <typename E, typename... R>
  void operator()(const E &err, R... results) noexcept {
    if (err) {
      obj.on_error(err, std::move(marker));
    } else {
      (obj.*impl_fn)(std::forward<R>(results)...);
    }
  }

  T &obj;
  M impl_fn;
  Marker marker;
};

template <typename T, typename M> struct forwarder_t<T, M, void> {
  forwarder_t(T &obj_, M &&member_)
      : obj{obj_}, impl_fn{std::forward<M>(member_)} {}

  template <typename E, typename... R>
  void operator()(const E &err, R... results) noexcept {
    if (err) {
      obj.on_error(err);
    } else {
      (obj.*impl_fn)(std::forward<R>(results)...);
    }
  }

  T &obj;
  M impl_fn;
};

template <typename T, typename M>
forwarder_t(T, M)->forwarder_t<std::decay_t<T>, M, void>;

template <typename T, typename M, typename Marker>
forwarder_t(T, M, Marker)
    ->forwarder_t<std::decay_t<T>, M, std::decay_t<Marker>>;

class my_simple_agent_t {
public:
  my_simple_agent_t(const std::string &host_, const std::string proto_,
                    boost::asio::io_context &io_context_)
      : host{host_}, proto{proto_},
        io_context{io_context_}, resolver{io_context}, skt{io_context} {}

  struct ctx_resolve {};
  struct ctx_connect {};

  void on_error(const boost::system::error_code &ec, ctx_resolve &&) {
    std::cout << "on_error[resolve_ctx]() :: " << ec.message() << "\n";
  }

  void on_error(const boost::system::error_code &ec, ctx_connect &&) {
    std::cout << "on_error[connect_ctx]() :: " << ec.message() << "\n";
  }

  void resolve() noexcept {
    std::cout << "resolve()\n";

    resolver.async_resolve(
        host, proto,
        forwarder_t(*this, &my_simple_agent_t::on_resolve, ctx_resolve{}));
  }

  void
  on_resolve(boost::asio::ip::tcp::resolver::results_type &&results) noexcept {
    std::cout << "on_resolve()\n";
    boost::asio::async_connect(
        skt, results.begin(), results.end(),
        forwarder_t(*this, &my_simple_agent_t::on_connect, ctx_connect{}));
  }

  void on_connect(boost::asio::ip::tcp::resolver::iterator it) noexcept {
    std::cout << "on_connect() :: " << it->endpoint() << "\n";
  }

private:
  std::string host;
  std::string proto;
  boost::asio::io_context &io_context;
  boost::asio::ip::tcp::resolver resolver;
  boost::asio::ip::tcp::socket skt;
};

class my_simple_agent2_t {
public:
  my_simple_agent2_t(const std::string &host_, const std::string proto_,
                     boost::asio::io_context &io_context_)
      : host{host_}, proto{proto_},
        io_context{io_context_}, resolver{io_context}, skt{io_context} {}

  void on_error(const boost::system::error_code &ec) {
    std::cout << "on_error2 [generic] " << ec.message() << "\n";
  }

  void resolve() noexcept {
    std::cout << "resolve2()\n";
    resolver.async_resolve(host, proto,
                           forwarder_t(*this, &my_simple_agent2_t::on_resolve));
  }

  void
  on_resolve(boost::asio::ip::tcp::resolver::results_type &&results) noexcept {
    std::cout << "on_resolve2()\n";
    boost::asio::async_connect(
        skt, results.begin(), results.end(),
        forwarder_t(*this, &my_simple_agent2_t::on_connect));
  }

  void on_connect(boost::asio::ip::tcp::resolver::iterator it) noexcept {
    std::cout << "on_connect2() :: " << it->endpoint() << "\n";
  }

private:
  std::string host;
  std::string proto;
  boost::asio::io_context &io_context;
  boost::asio::ip::tcp::resolver resolver;
  boost::asio::ip::tcp::socket skt;
};

class my_simple_agent3_t {
public:
  my_simple_agent3_t(const std::string &host_, const std::string proto_,
                     boost::asio::io_context &io_context_)
      : host{host_}, proto{proto_},
        io_context{io_context_}, resolver{io_context}, skt{io_context} {}

  void on_error(const boost::system::error_code &ec, const char *ctx) {
    std::cout << "on_error3 [" << ctx << "] :: " << ec.message() << "\n";
  }

  void resolve() noexcept {
    std::cout << "resolve3()\n";
    resolver.async_resolve(
        host, proto,
        forwarder_t(*this, &my_simple_agent3_t::on_resolve, "resolve"));
  }

  void
  on_resolve(boost::asio::ip::tcp::resolver::results_type &&results) noexcept {
    std::cout << "on_resolve3()\n";
    boost::asio::async_connect(
        skt, results.begin(), results.end(),
        forwarder_t(*this, &my_simple_agent3_t::on_connect, "connect"));
  }

  void on_connect(boost::asio::ip::tcp::resolver::iterator it) noexcept {
    std::cout << "on_connect3() :: " << it->endpoint() << "\n";
  }

private:
  std::string host;
  std::string proto;
  boost::asio::io_context &io_context;
  boost::asio::ip::tcp::resolver resolver;
  boost::asio::ip::tcp::socket skt;
};

int main(int argc, char **argv) {

  boost::asio::io_context io_context;
  try {
    std::string host = "yandex.by";
    std::string service = "httz";

    my_simple_agent_t agent{host, service, io_context};
    my_simple_agent2_t agent2{host, service, io_context};
    my_simple_agent3_t agent3{host, service, io_context};

    agent.resolve();
    agent2.resolve();
    agent3.resolve();

    io_context.run();
  } catch (const std::exception &ex) {
    std::cout << "exception : " << ex.what();
  }

  return 0;
}
