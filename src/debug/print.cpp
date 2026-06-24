//  Copyright (c) 2019-2020 John Biddiscombe
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "debug/print.hpp"
#if __has_include(<pika/debugging/print.hpp>)
// everythihg should be setup by pika
#else
//
# include <boost/crc.hpp>
# include <fmt/format.h>
//
# include <algorithm>
# include <array>
# include <atomic>
# include <bitset>
# include <chrono>
# include <climits>
# include <cmath>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <functional>
# include <iomanip>
# include <iostream>
# include <iterator>
# include <sstream>
# include <string>
# include <thread>
# include <type_traits>
# include <utility>
# include <vector>

# if defined(__FreeBSD__)
PIKA_EXPORT char** freebsd_environ = nullptr;
# endif

// ------------------------------------------------------------
/// \cond NODETAIL
namespace MARTYCAM_DETAIL_NS_DEBUG {
  using namespace martycam::debug::detail;

  // ------------------------------------------------------------------
  // format as ip address
  // ------------------------------------------------------------------
  ipaddr::ipaddr(void const* a /**/)
    : data_(reinterpret_cast<std::uint8_t const*>(a))
    , ipdata_(0)
  {
  }

  ipaddr::ipaddr(std::uint32_t a)
    : data_(reinterpret_cast<uint8_t const*>(&ipdata_))
    , ipdata_(a)
  {
  }

  std::ostream& operator<<(std::ostream& os, ipaddr const& p)
  {
    os << std::dec << int(p.data_[0]) << "." << int(p.data_[1]) << "." << int(p.data_[2]) << "."
       << int(p.data_[3]);
    return os;
  }

  // ------------------------------------------------------------------
  // helper class for printing time since start
  // ------------------------------------------------------------------
  std::ostream& operator<<(std::ostream& os, current_time_print_helper const&)
  {
    static std::chrono::steady_clock::time_point log_t_start = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();
    auto nowt = std::chrono::duration_cast<std::chrono::microseconds>(now - log_t_start).count();

    os << ffmt<dec10>(nowt) << " ";
    return os;
  }

  ///////////////////////////////////////////////////////////////////////////
  std::function<void(std::ostream&)>& get_print_info() noexcept
  {
    static std::function<void(std::ostream&)> print_info;
    return print_info;
  }

  void register_print_info(void (*printer)(std::ostream&)) { get_print_info() = printer; }

  void generate_prefix(std::ostream& os)
  {
# ifdef PIKA_DEBUG_PRINT_SHOW_TIME
    os << detail::current_time_print_helper();
# endif
    if (auto& f = get_print_info()) { f(os); }
  // os << detail::hostname_print_helper();
  }

  // ------------------------------------------------------------------
  // helper function for printing short memory dump and crc32
  // useful for debugging corruptions in buffers during
  // rma or other transfers
  // ------------------------------------------------------------------
  std::uint32_t crc32(void const* ptr, std::size_t size)
  {
    boost::crc_32_type result;
    result.process_bytes(ptr, size);
    return result.checksum();
  }

  mem_crc32::mem_crc32(void const* a, std::size_t len, std::size_t wrap)
    : addr_(reinterpret_cast<uint64_t const*>(a))
    , len_(len)
    , wrap_(wrap)
  {
  }

  std::ostream& operator<<(std::ostream& os, mem_crc32 const& p)
  {
    std::uint64_t const* uintBuf = static_cast<std::uint64_t const*>(p.addr_);
    os << "Memory:";
    os << " address " << fmt::ptr(p.addr_) << " length " << ffmt<hex6>(p.len_)
       << " CRC32:" << ffmt<hex8>(detail::crc32(p.addr_, p.len_)) << "\n";

    for (std::size_t i = 0;
         i < (std::min)(size_t(std::ceil(static_cast<double>(p.len_) / 8.0)), std::size_t(128));
         i++)
    {
      os << ffmt<hex16>(*uintBuf++) << " ";
      if (i % p.wrap_ == (p.wrap_ - 1)) os << "\n";
    }
    return os;
  }

  // ------------------------------------------------------------------
  // helper class for printing time since start
  // ------------------------------------------------------------------
  #if 0
  char const* hostname_print_helper::get_hostname_and_rank() const
  {
    static bool initialized = false;
    static char hostname_[20] = {'\0'};
    if (!initialized)
    {
      initialized = true;
# if !defined(__FreeBSD__)
      gethostname(hostname_, std::size_t(12));
# endif
      std::ostringstream temp;
      temp << '(' << std::to_string(guess_rank()) << ')';
      std::strcat(hostname_, temp.str().c_str());
    }
    return hostname_;
  }

  char const* hostname_print_helper::get_hostname() const
  {
    static bool initialized = false;
    static char hostname_[20] = {'\0'};
    if (!initialized)
    {
      initialized = true;
# if !defined(__FreeBSD__)
      gethostname(hostname_, std::size_t(12));
# endif
    }
    return hostname_;
  }

  int hostname_print_helper::guess_rank() const
  {
# if defined(__FreeBSD__)
    char** env = freebsd_environ;
# else
    char** env = environ;
# endif
    std::vector<std::string_view> env_strings{"_PROCID=", "_WORLD_RANK=", "_RANK="};

    for (auto s : env_strings)
    {
      for (char** current = env; *current; current++)
      {
        auto e = std::string(*current);
        auto pos = e.find(s);
        if (pos != std::string::npos)
        {
          //std::cout << "Got a rank string : " << e << std::endl;
          return std::stoi(e.substr(pos + s.size(), 5));
        }
      }
    }
    return -1;
  }

  std::ostream& operator<<(std::ostream& os, hostname_print_helper const& h)
  {
    os << ffmt<s12>(h.get_hostname_and_rank()) << " ";
    return os;
  }
#endif

  ///////////////////////////////////////////////////////////////////////
  template <typename T>
  GROX_EXPORT void print_array(std::string const& name, T const* data, std::size_t size)
  {
    std::cout << ffmt<s20>(name.c_str()) << ": {" << ffmt<dec4>(size) << "} : ";
    std::copy(data, data + size, std::ostream_iterator<T>(std::cout, ", "));
    std::cout << "\n";
  }

  template GROX_EXPORT void print_array(std::string const&, std::size_t const*, std::size_t);
}    // namespace MARTYCAM_DETAIL_NS_DEBUG
/// \endcond
#endif
