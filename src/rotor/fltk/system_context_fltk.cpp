//
// Copyright (c) 2025 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/fltk/system_context_fltk.h"
#include "rotor/supervisor.h"
#include <atomic>
#include <cstdint>

#include <FL/Fl.H>

using namespace rotor;
using namespace rotor::fltk;

namespace {
namespace to {
struct inbound_queue {};
struct queue {};
} // namespace to
} // namespace

template <> inline auto &rotor::supervisor_t::access<to::inbound_queue>() noexcept { return inbound_queue; }
template <> inline auto &rotor::supervisor_t::access<to::queue>() noexcept { return queue; }

static std::atomic_int32_t queue_counter{0};

static void _callback(void *data) {
    auto sup_raw = static_cast<supervisor_t *>(data);
    auto &inbound = sup_raw->access<to::inbound_queue>();
    auto &queue = sup_raw->access<to::queue>();
    message_base_t *ptr;
    bool try_fetch = true;
    while (try_fetch) {
        std::int32_t fetched = 0;
        try_fetch = false;
        while (inbound.pop(ptr)) {
            queue.emplace_back(ptr, false);
            ++fetched;
        }
        if (fetched) {
            sup_raw->do_process();
            try_fetch = queue_counter.fetch_sub(fetched) != fetched;
        }
    }
}

void system_context_fltk_t::enqueue(message_ptr_t message) noexcept {
    auto sup = get_supervisor().get();
    if (sup) {
        auto &inbound = sup->access<to::inbound_queue>();
        inbound.push(message.detach());
        if (queue_counter.fetch_add(1) == 0) {
            Fl::awake(_callback, sup);
        }
    }
}
