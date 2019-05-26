#include "rotor/asio/supervisor_asio.h"

using namespace rotor::asio;

void supervisor_asio_t::start() noexcept {
  auto actor_ptr = supervisor_ptr_t(this);
  asio::defer(strand, [self = std::move(actor_ptr)]() {
    self->do_start();
    self->process();
  });
}

void supervisor_asio_t::shutdown() noexcept {
  auto actor_ptr = supervisor_ptr_t(this);
  asio::defer(strand, [self = std::move(actor_ptr)]() {
    self->do_shutdown();
    self->process();
  });
}
