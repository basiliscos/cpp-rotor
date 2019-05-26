#include "rotor/actor_base.h"
#include "rotor/supervisor.h"

using namespace rotor;

actor_base_t::actor_base_t(supervisor_t &supervisor_)
    : supervisor{supervisor_} {}

actor_base_t::~actor_base_t() {}

void actor_base_t::do_initialize() noexcept { address = create_address(); }

address_ptr_t actor_base_t::create_address() noexcept {
  auto addr = new address_t(static_cast<void *>(&supervisor));
  return address_ptr_t{addr};
}

void actor_base_t::on_initialize(
    message_t<payload::initialize_actor_t> &) noexcept {}

void actor_base_t::on_shutdown(
    message_t<payload::shutdown_request_t> &) noexcept {
  auto destination = supervisor.get_address();
  send<payload::shutdown_confirmation_t>(destination, this);
}

void actor_base_t::remember_subscription(
    const subscription_request_t &req) noexcept {
  points.emplace(req.address, req.handler);
}

void actor_base_t::forget_subscription(
    const subscription_request_t &req) noexcept {
  auto it = points.begin();
  while (it != points.end()) {
    if (it->first == req.address && it->second == req.handler) {
      it = points.erase(it);
    } else {
      ++it;
    }
  }
}
