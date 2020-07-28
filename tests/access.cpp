//
// Copyright (c) 2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "access.h"

namespace rotor::test {

bool empty(rotor::subscription_t &subs) noexcept {
    return subs.access<to::internal_infos>().empty() && subs.access<to::mine_handlers>().empty();
}

} // namespace rotor::test
