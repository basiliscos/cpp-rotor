#pragma once

#pragma once

#include "rotor/supervisor.h"

namespace rotor {
namespace test {

struct supervisor_test_t : public supervisor_t {
    supervisor_test_t(supervisor_t *sup = nullptr);

    virtual void start_shutdown_timer() noexcept override;
    virtual void cancel_shutdown_timer() noexcept override;
    virtual void start() noexcept override;
    virtual void shutdown() noexcept override;
};


} // namespace test
} // namespace rotor
