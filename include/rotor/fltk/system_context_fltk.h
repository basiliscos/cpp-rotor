#pragma once

//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/arc.hpp"
#include "rotor/system_context.h"
#include "rotor/fltk/export.h"

#include <mutex>
#include <list>
#include <thread>

namespace rotor {
namespace fltk {

struct supervisor_fltk_t;
struct thread_messsage_t;

struct ROTOR_FLTK_API system_context_fltk_t: system_context_t{
    system_context_fltk_t();
    ~system_context_fltk_t();

    bool try_process(void* thead_message) noexcept;

    protected:
    using mutex_t = std::recursive_mutex;
    using theaded_messages_t = std::list<thread_messsage_t*>;
    using thread_id_t = std::thread::id;

    void enqueue_message(supervisor_fltk_t* supervisor, message_ptr_t message) noexcept;

    thread_id_t main_thread;
    mutex_t mutex;
    theaded_messages_t theaded_messages;

    friend struct supervisor_fltk_t;
};

/** \brief intrusive pointer type for fltk system context */
using system_context_ptr_t = rotor::intrusive_ptr_t<system_context_fltk_t>;

} // namespace fltk
} // namespace rotor
