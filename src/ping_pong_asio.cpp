#include <boost/asio.hpp>
#include <functional>
#include <iostream>
#include <type_traits>
#include <utility>

struct ping_t;
struct pong_t {
    boost::asio::io_context &io_context;
    ping_t *ping;
    int count;
    pong_t(boost::asio::io_context &io_context_, int count_) : io_context{io_context_}, count{count_} {}
    void operator()();
};

struct ping_t {
    boost::asio::io_context &io_context;
    pong_t *pong;
    int count;
    ping_t(boost::asio::io_context &io_context_, int count_) : io_context{io_context_}, count{count_} {}
    void operator()() {
        if (count) {
            // std::cout << "ping " << count << "\n";
            --count;
            boost::asio::post(io_context, [&]() { (*pong)(); });
        }
    }
};

void pong_t::operator()() {
    if (count) {
        --count;
        // std::cout << "pong " << count << "\n";
        boost::asio::post(io_context, [&]() { (*ping)(); });
    }
}

int main(int argc, char **argv) {

    boost::asio::io_context io_context{1};
    try {
        ping_t ping(io_context, 10000000);
        pong_t pong(io_context, 10000000);
        ping.pong = &pong;
        pong.ping = &ping;

        ping();

        io_context.run();

        std::cout << "pings left: " << ping.count << ", pongs left: " << pong.count << "\n";

    } catch (const std::exception &ex) {
        std::cout << "exception : " << ex.what();
    }

    return 0;
}
