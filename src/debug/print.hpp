#pragma once

#if __has_include(<pika/debugging/print.hpp>)
# include <pika/debugging/print.hpp>

namespace martycam::debug {
  using namespace pika::debug;
}    // namespace martycam::debug

namespace martycam::debug::detail {
  using namespace pika::debug::detail;
}

#elif __has_include(<fmt/format.h>)

# include <array>
# include <atomic>
# include <chrono>
# include <cstddef>
# include <cstdint>
# include <iomanip>
# include <iostream>
# include <sstream>
# include <string>
# include <tuple>
# include <type_traits>
# include <utility>
# include <vector>
//
# include <fmt/format.h>
# define GROX_EXPORT __attribute__((visibility("default")))
// # include "config/export_definitions.hpp"

// ------------------------------------------------------------
// This file provides a simple to use printf style debugging
// tool that can be used on a per file basis to enable output.
// It is not intended to be exposed to users, but rather as
// an aid for internal development.
// ------------------------------------------------------------
// Usage: Instantiate a debug print object at the top of a file
// using a template param of true/false to enable/disable output.
// When the template parameter is false, the optimizer will
// not produce code and so the impact is nil.
//
// static martycam::debug::detail::enable_print<true> spq_deb("SUBJECT");
//
// Later in code you may print information using
//
//             spq_deb.debug(ffmt<s16>("cleanup_terminated"), "v1"
//                  , "D" , dec<2>(domain_num)
//                  , "Q" , ffmt<dec3>(q_index)
//                  , "thread_num", ffmt<dec3>(local_num));
//
// various print formatters (dec/hex/str) are supplied to make
// the output regular and aligned for easy parsing/scanning.
//
// In tight loops, huge amounts of debug information might be
// produced, so a simple timer based output is provided
// To instantiate a timed output
//      static auto getnext = spq_deb.make_timer(1
//              , ffmt<s16>("get_next_thread"));
// then inside a tight loop
//      spq_deb.timed(getnext, dec<>(thread_num));
// The output will only be produced every N seconds
// ------------------------------------------------------------

// Used to wrap function call parameters to prevent evaluation
// when debugging is disabled
# define GROX_DETAIL_DP_LAZY(printer, Expr) printer.eval([&] { return Expr; })
# define GROX_DETAIL_DP(printer, Expr)                                                             \
   /*if constexpr (printer.is_enabled())*/ {                                                       \
     using namespace martycam::debug::detail;                                                      \
     printer.Expr;                                                                                 \
   };

# define MARTYCAM_DETAIL_NS_DEBUG martycam::debug::detail

// ------------------------------------------------------------
/// \cond NODETAIL
// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace MARTYCAM_DETAIL_NS_DEBUG {
  // common formats that are used with acceptable alignment
  constexpr char bin8[] = "{:08b}";
  constexpr char bin16[] = "{:016b}";
  constexpr char dec3[] = "{:03d}";
  constexpr char dec4[] = "{:04d}";
  constexpr char dec6[] = "{:06d}";
  constexpr char dec8[] = "{:08d}";
  constexpr char dec9[] = "{:09d}";
  constexpr char dec10[] = "{:010d}";
  constexpr char dec12[] = "{:012d}";
  constexpr char dec18[] = "{:018d}";
  constexpr char hex6[] = "{:#08x}";       // add 2 for 0x prefix
  constexpr char hex8[] = "{:#010x}";      // ...
  constexpr char hex12[] = "{:#014x}";     // ...
  constexpr char hex16[] = "{:#018x}";     // ...
  constexpr char fp12_8[] = "{:12.8f}";    // a commmon layout
  constexpr char strl[] = "{:<{}}";
  constexpr char strr[] = "{:>{}}";
  constexpr char s20[] = "{:>20}";

  // ------------------------------------------------------------------
  // helper for N>M true/false
  // ------------------------------------------------------------------
  template <int Level, int Threshold>
  struct check_level : std::integral_constant<bool, Level <= Threshold>
  {
  };

  // ------------------------------------------------------------------
  // format using fmt::format
  // ------------------------------------------------------------------
  template <char const* fmt_str>
  struct ffmt
  {
    template <typename T>
    ffmt(T const& val)
      : fmt_(fmt::format(fmt_str, val))
    {
    }

    template <typename T>
    ffmt(std::atomic<T> const& val)
      : fmt_(fmt::format(fmt_str, val.load()))
    {
    }

    std::string const fmt_;

    constexpr friend std::ostream& operator<<(std::ostream& os, ffmt const& d)
    {
      return os << d.fmt_;
    }
  };

  // ------------------------------------------------------------------
  // format as padded string
  // ------------------------------------------------------------------
  template <int N = 20>
  struct str
  {
    str(char const* val)
      : fmt_(fmt::format(strl, val, N))
    {
    }

    std::string const fmt_;

    friend std::ostream& operator<<(std::ostream& os, str<N> const& d) { return os << d.fmt_; }
  };

  // ------------------------------------------------------------------
  // format as ip address
  // ------------------------------------------------------------------
  struct ipaddr
  {
    GROX_EXPORT ipaddr(void const* a);
    GROX_EXPORT ipaddr(std::uint32_t a);

    std::uint8_t const* data_;
    std::uint32_t const ipdata_;

    GROX_EXPORT friend std::ostream& operator<<(std::ostream& os, ipaddr const& p);
  };

  // ------------------------------------------------------------------
  // helper class for printing time since start
  // ------------------------------------------------------------------
  struct current_time_print_helper
  {
    GROX_EXPORT friend std::ostream& operator<<(std::ostream& os, current_time_print_helper const&);
  };

  // ------------------------------------------------------------------
  // helper function for printing CRC32
  // ------------------------------------------------------------------
  std::uint32_t crc32(void const* ptr, std::size_t size);

  // ------------------------------------------------------------------
  // helper function for printing short memory dump and crc32
  // useful for debugging corruptions in buffers during
  // rma or other transfers
  // ------------------------------------------------------------------
  struct mem_crc32
  {
    GROX_EXPORT mem_crc32(void const* a, std::size_t len, std::size_t wrap = 8);

    std::uint64_t const* addr_;
    std::size_t const len_;
    std::size_t const wrap_;

    GROX_EXPORT friend std::ostream& operator<<(std::ostream& os, mem_crc32 const& p);
  };

  template <typename TupleType, std::size_t... I>
  void tuple_print(std::ostream& os, TupleType const& t, std::index_sequence<I...>)
  {
    (..., (os << (I == 0 ? "" : " ") << std::get<I>(t)));
  }

  template <typename... Args>
  void tuple_print(std::ostream& os, std::tuple<Args...> const& t)
  {
    tuple_print(os, t, std::make_index_sequence<sizeof...(Args)>());
  }

  // ------------------------------------------------------------------
  // helper class for printing time since start
  // ------------------------------------------------------------------
#if 0
  struct hostname_print_helper
  {
    GROX_EXPORT char const* get_hostname_and_rank() const;
    GROX_EXPORT char const* get_hostname() const;
    GROX_EXPORT int guess_rank() const;

    GROX_EXPORT friend std::ostream& operator<<(std::ostream& os, hostname_print_helper const& h);
  };
#endif

  ///////////////////////////////////////////////////////////////////////
  GROX_EXPORT void register_print_info(void (*)(std::ostream&));
  GROX_EXPORT void generate_prefix(std::ostream& os);

  ///////////////////////////////////////////////////////////////////////
  template <typename... Args>
  void display(char const* prefix, Args const&... args)
  {
    // using a temp stream object with a single copy to cout at the end
    // prevents multiple threads from injecting overlapping text
    std::stringstream tempstream;
    tempstream << prefix;
    generate_prefix(tempstream);
    ((tempstream << args << " "), ...);
    tempstream << "\n";
    std::cout << tempstream.str() << std::flush;
  }

  template <typename... Args>
  void debug_impl(Args const&... args)
  {
    display("<DEB> ", args...);
  }

  template <typename... Args>
  void warning_impl(Args const&... args)
  {
    display("<WAR> ", args...);
  }

  template <typename... Args>
  void error_impl(Args const&... args)
  {
    display("<ERR> ", args...);
  }

  template <typename... Args>
  void scope(Args const&... args)
  {
    display("<SCO> ", args...);
  }

  template <typename... Args>
  void trace_impl(Args const&... args)
  {
    display("<TRC> ", args...);
  }

  template <typename... Args>
  void timed_impl(Args const&... args)
  {
    display("<TIM> ", args...);
  }

  template <typename... Args>
  struct scoped_var
  {
    // capture tuple elements by reference - no temp vars in constructor please
    char const* prefix_;
    std::tuple<Args const&...> const message_;
    std::string buffered_msg;

    //
    scoped_var(char const* p, Args const&... args)
      : prefix_(p)
      , message_(args...)
    {
      std::stringstream tempstream;
      tuple_print(tempstream, message_);
      buffered_msg = tempstream.str();
      display("<SCO> ", prefix_, ffmt<s20>(">> enter <<"), tempstream.str());
    }

    ~scoped_var() { display("<SCO> ", prefix_, ffmt<s20>("<< leave >>"), buffered_msg); }
  };

  struct empty_timed_var
  {
    constexpr bool trigger() const { return false; }

    constexpr double elapsed() const { return 0; }
  };

  template <typename... Args>
  struct timed_var
  {
    mutable std::chrono::steady_clock::time_point time_start_;
    mutable std::chrono::steady_clock::time_point time_check_;
    double const delay_;
    std::tuple<Args...> const message_;
    //
    timed_var(double const& delay, Args const&... args)
      : time_start_(std::chrono::steady_clock::now())
      , time_check_(time_start_)
      , delay_(delay)
      , message_(args...)
    {
    }

    bool trigger() const
    {
      auto now = std::chrono::steady_clock::now();
      double elapsed_ =
          std::chrono::duration_cast<std::chrono::duration<double>>(now - time_check_).count();

      if (elapsed_ > delay_)
      {
        time_check_ = now;
        return true;
      }
      return false;
    }

    double elapsed() const
    {
      return std::chrono::duration_cast<std::chrono::duration<double>>(
          std::chrono::steady_clock::now() - time_start_)
          .count();
    }

    friend std::ostream& operator<<(std::ostream& os, timed_var<Args...> const& ti)
    {
      tuple_print(os, ti.message_);
      return os;
    }
  };

  ///////////////////////////////////////////////////////////////////////////
  template <bool enable>
  struct enable_print;

  // when false, debug statements should produce no code
  template <>
  struct enable_print<false>
  {
    constexpr enable_print(char const*) {}

    constexpr bool is_enabled() const { return false; }

    template <typename... Args>
    constexpr void debug(Args const&...) const
    {
    }

    template <typename... Args>
    constexpr void warning(Args const&...) const
    {
    }

    template <typename... Args>
    constexpr void trace(Args const&...) const
    {
    }

    template <typename... Args>
    constexpr void error(Args const&...) const
    {
    }

    template <typename... Args>
    constexpr void timed(Args const&...) const
    {
    }

    template <typename T>
    constexpr void array(std::string const&, std::vector<T> const&) const
    {
    }

    template <typename T, std::size_t N>
    constexpr void array(std::string const&, std::array<T, N> const&) const
    {
    }

    template <typename T>
    constexpr void array(std::string const&, T const*, std::size_t) const
    {
    }

    template <typename... Args>
    constexpr bool scope(Args const&...)
    {
      return true;
    }

    template <typename T, typename... Args>
    constexpr bool declare_variable(Args const&...) const
    {
      return true;
    }

    template <typename T, typename V>
    static constexpr void set(T&, V const&)
    {
    }

    // @todo, return void so that timers have zero footprint when disabled
    template <typename... Args>
    constexpr empty_timed_var make_timer(double const, Args const&...) const
    {
      return empty_timed_var{};
    }

    template <typename Expr>
    constexpr bool eval(Expr const&) const
    {
      return true;
    }
  };

  template <typename T>
  GROX_EXPORT void print_array(std::string const& name, T const* data, std::size_t size);

  // when true, debug statements produce valid output
  template <>
  struct enable_print<true>
  {
private:
    char const* prefix_;

public:
    constexpr enable_print()
      : prefix_("")
    {
    }

    constexpr enable_print(char const* p)
      : prefix_(p)
    {
    }

    constexpr bool is_enabled() const { return true; }

    template <typename... Args>
    constexpr void debug(Args const&... args) const
    {
      debug_impl(prefix_, args...);
    }

    template <typename... Args>
    constexpr void warning(Args const&... args) const
    {
      warning_impl(prefix_, args...);
    }

    template <typename... Args>
    constexpr void trace(Args const&... args) const
    {
      trace_impl(prefix_, args...);
    }

    template <typename... Args>
    constexpr void error(Args const&... args) const
    {
      error_impl(prefix_, args...);
    }

    template <typename... Args>
    scoped_var<Args...> scope(Args const&... args)
    {
      return scoped_var<Args...>(prefix_, args...);
    }

    template <typename... T, typename... Args>
    void timed(timed_var<T...> const& init, Args const&... args) const
    {
      if (init.trigger()) { timed_impl(prefix_, init, args...); }
    }

    template <typename T>
    void array(std::string const& name, std::vector<T> const& v) const
    {
      print_array(name, v.data(), v.size());
    }

    template <typename T, std::size_t N>
    void array(std::string const& name, std::array<T, N> const& v) const
    {
      print_array(name, v.data(), N);
    }

    template <typename T>
    void array(std::string const& name, T const* data, std::size_t size) const
    {
      print_array(name, data, size);
    }

    template <typename T, typename... Args>
    T declare_variable(Args const&... args) const
    {
      return T(args...);
    }

    template <typename T, typename V>
    static void set(T& var, V const& val)
    {
      var = val;
    }

    template <typename... Args>
    constexpr timed_var<Args...> make_timer(double const delay, Args const... args) const
    {
      return timed_var<Args...>(delay, args...);
    }

    template <typename Expr>
    auto eval(Expr const& e) const
    {
      return e();
    }
  };

  template <int Level, int Threshold>
  struct print_threshold : enable_print<check_level<Level, Threshold>::value>
  {
    using base_type = enable_print<check_level<Level, Threshold>::value>;
    // inherit constructor
    using base_type::base_type;
  };
}    // namespace MARTYCAM_DETAIL_NS_DEBUG
/// \endcond
#else
# error "No suitable debug print implementation found"
#endif
