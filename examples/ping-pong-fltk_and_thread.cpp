#include <rotor/fltk.hpp>
#include <rotor/thread.hpp>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>

#ifndef _WIN32
#include <X11/Xlib.h>
#include <cstdlib>
#endif

namespace r = rotor;
namespace rf = r::fltk;
namespace rt = r::thread;

namespace payload {

struct pong_t {};
struct ping_t {
    using response_t = pong_t;
};

} // namespace payload

namespace message {

using ping_t = r::request_traits_t<payload::ping_t>::request::message_t;
using pong_t = r::request_traits_t<payload::ping_t>::response::message_t;

} // namespace message

struct pinger_t : public r::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
        plugin.with_casted<r::plugin::registry_plugin_t>([&](auto &p) { p.discover_name("ponger", ponger_addr, true).link(); });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        auto timeout = r::pt::seconds{2};
        start_timer(timeout, *this, &pinger_t::on_ping_timer);
    }

    void on_ping_timer(r::request_id_t, bool) noexcept {
        box->label("ping has been sent");
        auto timeout = r::pt::seconds{3};
        request<payload::ping_t>(ponger_addr).send(timeout);
    }

    void on_pong(message::pong_t &) noexcept {
        box->label("pong has been received, going to shutdown");
        auto timeout = r::pt::seconds{1};
        start_timer(timeout, *this, &pinger_t::on_shutdown_timer);
    }

    void on_shutdown_timer(r::request_id_t, bool) noexcept { supervisor->do_shutdown(); }

    rotor::address_ptr_t ponger_addr;
    Fl_Box *box;
};

struct ponger_t : r::actor_base_t {
    using rotor::actor_base_t::actor_base_t;
    using ping_message_ptr_t = r::intrusive_ptr_t<message::ping_t>;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
        plugin.with_casted<rotor::plugin::registry_plugin_t>(
            [&](auto &p) { p.register_name("ponger", get_address()); });
    }

    void on_ping(message::ping_t &message) noexcept {
        auto timeout = r::pt::seconds{2};
        request = &message;
        start_timer(timeout, *this, &ponger_t::on_timer);
    }

    void on_timer(r::request_id_t, bool) noexcept {
        reply_to(*request);
        request.reset();
    }

    ping_message_ptr_t request;
};

bool is_display_available() {
#ifndef _WIN32
    char *disp = getenv("DISPLAY");
    if (disp == nullptr)
        return false;
    Display *dpy = XOpenDisplay(disp);
    if (dpy == nullptr)
        return false;
    XCloseDisplay(dpy);
#endif
    return true;
}

int main(int argc, char **argv) {
    if (!is_display_available()) {
        return 0;
    }

    // let fltk-core be aware of lockin mechanism (i.e. Fl::awake will work)
    Fl::lock();

    Fl_Window window = Fl_Window(400, 180);
    Fl_Box *box = new Fl_Box(20, 40, 360, 100, "auto shutdown in 5 seconds");
    box->box(FL_UP_BOX);
    window.end();
    window.show(argc, argv);

    auto fltk_context = rf::system_context_fltk_t();
    auto timeout = r::pt::millisec{500};
    auto fltk_supervisor = fltk_context.create_supervisor<rf::supervisor_fltk_t>().timeout(timeout).create_registry().finish();

    // warm-up
    fltk_supervisor->do_process();

    auto pinger = fltk_supervisor->create_actor<pinger_t>().timeout(timeout).finish();
    pinger->box = box;

    auto thread_context = rt::system_context_thread_t();
    auto thread_supervisor = thread_context.create_supervisor<rt::supervisor_thread_t>().timeout(timeout)
                                  .registry_address(fltk_supervisor->get_registry_address()).finish();
    auto ponger = thread_supervisor->create_actor<ponger_t>().timeout(timeout).finish();

    auto auxiliary_thread = std::thread([&](){
        thread_context.run();
    });

    Fl::add_check([](auto *data) { reinterpret_cast<r::supervisor_t *>(data)->do_process(); }, fltk_supervisor.get());

    while (!fltk_supervisor->get_shutdown_reason()) {
        fltk_supervisor->do_process();
        Fl::wait(0.1);
    }

    thread_supervisor->shutdown();
    auxiliary_thread.join();

    return 0;
}
