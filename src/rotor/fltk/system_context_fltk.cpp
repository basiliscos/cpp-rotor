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

    ~thread_messsage_t(){
        intrusive_ptr_release(supervisor);
    }
};

system_context_fltk_t::system_context_fltk_t() {
    main_thread = std::this_thread::get_id();
}

system_context_fltk_t::~system_context_fltk_t() {
    auto lock = std::lock_guard(mutex);
    while(!theaded_messages.empty()) {
        auto holder = theaded_messages.front();
        delete holder;
        theaded_messages.pop_front();
    }
}

void system_context_fltk_t::enqueue_message(supervisor_fltk_t* supervisor, message_ptr_t message) noexcept {
    if (std::this_thread::get_id() == main_thread){
        if (message) {
            supervisor->put(std::move(message));
        }
    }
    else {
        auto threaded_message = new thread_messsage_t(std::move(message), supervisor);
        auto lock = std::lock_guard(mutex);
        theaded_messages.emplace_back(threaded_message);
        if (theaded_messages.size() == 1) {
            Fl::awake(this);
        }
    }
}

bool system_context_fltk_t::try_process(void* thead_message) noexcept {
    bool result = false;
    auto supervisor = get_supervisor();
    if (thead_message == this) {
        result = true;
        auto lock = std::lock_guard(mutex);
        while(!theaded_messages.empty()) {
            auto holder = theaded_messages.front();
            if (holder->message) { supervisor->put(std::move(holder->message)); }
            delete holder;
            theaded_messages.pop_front();
        }
    }
    supervisor->do_process();
    return result;
}

}
