//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/asio.hpp"

namespace asio = boost::asio;
namespace pt = boost::posix_time;

struct server_actor : public rotor::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    void on_start(rotor::message_t<rotor::payload::start_actor_t> &) noexcept override {
        std::cout << "hello world\n";
        supervisor.do_shutdown(); // optional
    }
};

int main() {
    asio::io_context io_context;
    auto system_context = rotor::asio::system_context_asio_t::ptr_t{new rotor::asio::system_context_asio_t(io_context)};
    auto stand = std::make_shared<asio::io_context::strand>(io_context);
    auto timeout = boost::posix_time::milliseconds{500};
    rotor::asio::supervisor_config_asio_t conf{timeout, std::move(stand)};
    auto sup = system_context->create_supervisor<rotor::asio::supervisor_asio_t>(conf);

    auto hello = sup->create_actor<server_actor>(timeout);

    sup->start();
    io_context.run();
    return 0;
}
