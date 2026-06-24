#pragma once

#include <iostream>
//
#include <pika/debugging/backtrace.hpp>

// ----------------------------------------------------------------------------
inline void print_stacktrace()
{
  auto trace_ = pika::debug::detail::trace();
  std::cout << trace_;
}

inline std::string get_stacktrace()
{
  auto trace_ = pika::debug::detail::trace();
  return trace_;
}