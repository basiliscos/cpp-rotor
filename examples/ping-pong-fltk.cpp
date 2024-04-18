#include <rotor/fltk.hpp>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>

#ifndef _WIN32
#include <X11/Xlib.h>
#include <cstdlib>
#endif

namespace r = rotor;
namespace rf = r::fltk;

namespace payload {

struct ping_t {};
struct pong_t {};

} // namespace payload

namespace message {

using ping_t = r::message_t<payload::ping_t>;
using pong_t = r::message_t<payload::pong_t>;

} // namespace message

struct pinger_t : public r::actor_base_t {
    using rotor::actor_base_t::actor_base_t;

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        auto timeout = r::pt::seconds{2};
        start_timer(timeout, *this, &pinger_t::on_ping_timer);
    }

    void on_ping_timer(r::request_id_t, bool) noexcept {
        box->label("ping has been sent");
        send<payload::ping_t>(ponger_addr);
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

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&ponger_t::on_ping); });
    }

    void on_ping(message::ping_t &) noexcept {
        auto timeout = r::pt::seconds{2};
        start_timer(timeout, *this, &ponger_t::on_timer);
    }

    void on_timer(r::request_id_t, bool) noexcept { send<payload::pong_t>(pinger_addr); }

    rotor::address_ptr_t pinger_addr;
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

    auto context = rf::system_context_fltk_t();
    auto timeout = r::pt::millisec{50};
    auto supervisor = context.create_supervisor<rf::supervisor_fltk_t>().timeout(timeout).create_registry().finish();
    auto pinger = supervisor->create_actor<pinger_t>().timeout(timeout).finish();
    auto ponger = supervisor->create_actor<ponger_t>().timeout(timeout).finish();

    pinger->box = box;
    pinger->ponger_addr = ponger->get_address();
    ponger->pinger_addr = pinger->get_address();

    // warm-up, optional
    supervisor->do_process();

    Fl::add_check([](auto *data) { reinterpret_cast<r::supervisor_t *>(data)->do_process(); }, supervisor.get());

    while (Fl::wait() && !supervisor->get_shutdown_reason()) {
    }

    return 0;
}
