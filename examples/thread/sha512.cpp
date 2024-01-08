//
// Copyright (c) 2019-2024 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

/*
 * This is an example how to implement interruptible blocking operations using
 * std::thread backend. Here, as blocking I/O operation the the reading disk
 * file and calculating its sha512 digest is used.
 *
 * The whole work is split into pieces, and once a piece is complete the
 * continuation message (with the whole job state) is send to the next
 * piece to be processed and so on. Between continuation messages other
 * messages might appear (in the case, the shutdown message) or timers
 * might be triggered.
 *
 * This is an example of blocking messages multiplexing pattern.
 *
 * The "ctrl+c" can be anytime pressed on the terminal, and the program
 * will correctly shutdown (including sanitizer build). Try it!
 *
 * As the additional std::threads are not spawned, this example is
 * ok to compile it with BUILD_THREAD_UNSAFE mode.
 *
 */

#include "rotor.hpp"
#include "rotor/thread.hpp"
#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>
#include <memory>
#include <atomic>
#include <openssl/sha.h>
#include <openssl/evp.h>

#ifndef _WIN32
#include <signal.h>
#else
#include <windows.h>
#endif

namespace r = rotor;
namespace rth = rotor::thread;

using buffer_t = std::vector<std::byte>;
template <typename T> using guard_t = std::unique_ptr<T, std::function<void(T *)>>;

enum class work_result_t { done, completed, errored };

struct work_t {
    using evp_ctx_t = guard_t<EVP_MD_CTX>;

    work_t(std::ifstream &&in_, size_t file_size_, size_t buff_sz_)
        : in(std::move(in_)), file_size{file_size_}, buff(buff_sz_) {
        evp_ctx = evp_ctx_t(EVP_MD_CTX_new(), [](auto ptr) { EVP_MD_CTX_free(ptr); });
        if (EVP_DigestInit_ex(evp_ctx.get(), EVP_sha512(), NULL) != 1) {
            error = "fail to init sha";
        }
    }

    std::string get_error() const noexcept {
        assert(error.size() && "has error");
        return error;
    }

    std::string get_result() const noexcept {
        assert(error.size() == 0 && "has no error");
        assert(result.size() != 0 && "has result");
        return result;
    }

    work_result_t io() noexcept {
        if (error.size()) {
            return work_result_t::errored;
        }
        auto bytes_left = file_size - bytes_read;
        auto final = bytes_left < buff.size();
        auto bytes_to_read = final ? bytes_left : buff.size();
        in.read(reinterpret_cast<char *>(buff.data()), bytes_to_read);
        if (!in) {
            error = "reading file error";
            return work_result_t::errored;
        }
        // printf("read %llu bytes\n", bytes_to_read);
        bytes_read += bytes_to_read;
        auto r = EVP_DigestUpdate(evp_ctx.get(), buff.data(), bytes_to_read);
        if (r != 1) {
            error = "sha update failed";
            return work_result_t::errored;
        }

        if (!final) {
            return work_result_t::done;
        }

        unsigned int trailing_bytes = SHA512_DIGEST_LENGTH;
        unsigned char digest[SHA512_DIGEST_LENGTH];
        r = EVP_DigestFinal_ex(evp_ctx.get(), digest, &trailing_bytes);
        if (r != 1) {
            error = "sha final failed";
            return work_result_t::errored;
        }
        result = std::string((char *)digest, trailing_bytes);
        return work_result_t::completed;
    }

  private:
    evp_ctx_t evp_ctx;
    std::ifstream in;
    size_t file_size;
    buffer_t buff;
    size_t bytes_read = 0;
    std::string error;
    std::string result;
};

namespace payload {
struct work_progress_t {
    std::unique_ptr<work_t> work;
};
} // namespace payload

namespace message {
using work_progress_t = r::message_t<payload::work_progress_t>;
}

struct sah_actor_config : r::actor_config_t {
    std::string path = "";
    std::size_t block_size = 0;
};

template <typename Actor> struct sah_actor_config_builder_t : r::actor_config_builder_t<Actor> {
    using builder_t = typename Actor::template config_builder_t<Actor>;
    using parent_t = r::actor_config_builder_t<Actor>;
    using parent_t::parent_t;

    builder_t &&path(const std::string &value) &&noexcept {
        parent_t::config.path = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }

    builder_t &&block_size(std::size_t value) &&noexcept {
        parent_t::config.block_size = value;
        return std::move(*static_cast<typename parent_t::builder_t *>(this));
    }
};

struct sha_actor_t : public r::actor_base_t {
    using config_t = sah_actor_config;
    template <typename Actor> using config_builder_t = sah_actor_config_builder_t<Actor>;

    explicit sha_actor_t(config_t &cfg) : r::actor_base_t{cfg}, path{cfg.path}, block_size{cfg.block_size} {}

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
            p.subscribe_actor(&sha_actor_t::on_process)->tag_io(); // important
        });
    }

    void on_start() noexcept override {
        rotor::actor_base_t::on_start();
        std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);
        if (!in.is_open()) {
            std::cout << "failed to open " << path << '\n';
            return do_shutdown();
        }

        auto sz = in.tellg();
        in = std::ifstream(path, std::ifstream::binary);
        auto work = std::make_unique<work_t>(std::move(in), sz, block_size);
        send<payload::work_progress_t>(address, payload::work_progress_t{std::move(work)});
    }

  private:
    std::string path;
    std::size_t block_size;

    void print_result(const work_t &work) noexcept {
        auto r = work.get_result();
        const std::byte *buff = reinterpret_cast<const std::byte *>(r.data());
        for (size_t i = 0; i < r.size(); ++i) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) << (unsigned)buff[i];
        }
        std::cout << "\n";
    }

    void on_process(message::work_progress_t &msg) noexcept {
        auto &work = msg.payload.work;
        auto result = msg.payload.work->io();
        switch (result) {
        case work_result_t::done:
            send<payload::work_progress_t>(address, payload::work_progress_t{std::move(work)});
            break;
        case work_result_t::errored:
            std::cout << "error: " << work->get_error() << "\n";
            supervisor->do_shutdown();
            break;
        case work_result_t::completed:
            print_result(*work);
            supervisor->do_shutdown();
            break;
        }
    }
};

std::atomic_bool shutdown_flag = false;

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        shutdown_flag = true;
    }
    return TRUE; /* ignore */
}
#endif

int main(int argc, char **argv) {
    std::string path = argv[0];
    if (argc < 2) {
        std::cout << "usage:: " << argv[0] << " /path/to/file [block_size = 1048576]\n";
        std::cout << "will calculate for " << argv[0] << "\n";
    } else {
        path = argv[1];
    }
    size_t block_size = 1048576;
    if (argc == 3) {
        try {
            block_size = static_cast<size_t>(std::stoll(argv[2]));
        } catch (...) {
            std::cout << "can't convert '" << argv[2] << "', using default one\n";
        }
    }

    rth::system_context_thread_t ctx;
    auto timeout = r::pt::milliseconds{100};
    auto sup = ctx.create_supervisor<rth::supervisor_thread_t>()
                   .timeout(timeout)
                   .shutdown_flag(shutdown_flag, timeout / 2)
                   .finish();
    sup->create_actor<sha_actor_t>()
        .block_size(block_size)
        .path(path)
        .timeout(timeout)
        .autoshutdown_supervisor()
        .finish();

#ifndef _WIN32
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = [](int) { shutdown_flag = true; };
    if (sigaction(SIGINT, &action, nullptr) != 0) {
        std::cout << "sigaction failed\n";
        return -1;
    }
#else
    if (!SetConsoleCtrlHandler(consoleHandler, true)) {
        std::cout << "SetConsoleCtrlHandler failed\n";
        return -1;
    }
#endif

    ctx.run();
    if (shutdown_flag) {
        std::cout << "terminated due to ctrl+c press\n";
    }

    std::cout << "normal exit\n";
    return 0;
}
