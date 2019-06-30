#include "rotor/asio.hpp"

namespace asio = boost::asio;
namespace pt = boost::posix_time;
namespace ra = rotor::asio;

struct hello_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void on_start(rotor::message_t<rotor::payload::start_actor_t> &) noexcept override {
        std::cout << "hello world\n";
        supervisor.do_shutdown();
    }
};

int main() {
    asio::io_context io_context;
    auto system_context = ra::system_context_asio_t::ptr_t{new ra::system_context_asio_t(io_context)};
    ra::supervisor_config_t conf{pt::milliseconds{500}};
    auto sup = system_context->create_supervisor<ra::supervisor_asio_t>(conf);

    auto hello = sup->create_actor<hello_actor>();

    sup->start();
    io_context.run();
    return 0;
}
