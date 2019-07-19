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
/** \brief thread-unsafe intrusive pointer policy */
using counter_policy_t = boost::thread_unsafe_counter;
#else
/** \brief thread-safe intrusive pointer policy */
using counter_policy_t = boost::thread_safe_counter;
#endif

/** \brief base class to inject ref-counter with the specified policiy */
template <typename T> using arc_base_t = boost::intrusive_ref_counter<T, counter_policy_t>;

/** \brief alias for intrusive pointer */
template <typename T> using intrusive_ptr_t = boost::intrusive_ptr<T>;

} // namespace rotor
