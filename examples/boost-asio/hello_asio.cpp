#include "rotor/asio.hpp"

namespace asio = boost::asio;
namespace pt = boost::posix_time;

struct hello_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void on_start(rotor::message_t<rotor::payload::start_actor_t> &) noexcept override {
        std::cout << "hello world\n";
        supervisor.do_shutdown(); // optional
    }
};

int main() {
    asio::io_context io_context;
    auto system_context = rotor::asio::system_context_asio_t::ptr_t{new rotor::asio::system_context_asio_t(io_context)};
    rotor::asio::supervisor_config_t conf{pt::milliseconds{500}};
    auto sup = system_context->create_supervisor<rotor::asio::supervisor_asio_t>(conf);

    auto hello = sup->create_actor<hello_actor>();

    sup->start();
    io_context.run();
    return 0;
}
