//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/fltk/system_context_fltk.h"
#include "rotor/fltk/supervisor_fltk.h"

#include <FL/Fl.H>

namespace rotor::fltk {

struct thread_messsage_t {
    message_ptr_t message;
    supervisor_fltk_t *supervisor;

    thread_messsage_t(message_ptr_t message_, supervisor_fltk_t *supervisor_): message{message_}, supervisor{supervisor_}{
        intrusive_ptr_add_ref(supervisor);
    }

    thread_messsage_t() = delete;
    thread_messsage_t(const thread_messsage_t&) = delete;
    thread_messsage_t(thread_messsage_t&&) = delete;

    ~thread_messsage_t(){
        intrusive_ptr_release(supervisor);
    }
};

void system_context_fltk_t::enqueue_message(supervisor_fltk_t* supervisor, message_ptr_t message) noexcept {
    auto msg = new thread_messsage_t(std::move(message), supervisor);
    Fl::awake([](void *data){
        auto msg = static_cast<thread_messsage_t*>(data);
        if (msg->message) {
            msg->supervisor->put(std::move(msg->message));
        }
        msg->supervisor->do_process();
        delete msg;
    }, msg);
}

} // namespace fltk
