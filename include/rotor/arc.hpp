#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

namespace rotor {

#ifdef ROTOR_REFCOUNT_THREADUNSAFE
using counter_policy_t = boost::thread_unsafe_counter;
#else
using counter_policy_t = boost::thread_safe_counter;
#endif

template <typename T> using arc_base_t = boost::intrusive_ref_counter<T, counter_policy_t>;

template <typename T> using intrusive_ptr_t = boost::intrusive_ptr<T>;

} // namespace rotor
