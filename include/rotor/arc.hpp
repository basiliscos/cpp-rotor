#pragma once

#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

namespace rotor {

using counter_policy_t = boost::thread_unsafe_counter;

template <typename T>
using arc_base_t = boost::intrusive_ref_counter<T, counter_policy_t>;

} // namespace rotor
