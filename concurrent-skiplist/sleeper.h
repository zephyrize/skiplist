#pragma once

#include <chrono>
#include <cstdint>
#include <thread>


namespace utility {
namespace skiplist {

//////////////////////////////////////////////////////////////////////

namespace detail {

inline void asm_volatile_pause() {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
  ::_mm_pause();
#elif defined(__i386__) || FOLLY_X64 || \
    (defined(__mips_isa_rev) && __mips_isa_rev > 1)
  asm volatile("pause");
#elif FOLLY_AARCH64
  asm volatile("isb");
#elif (defined(__arm__) && !(__ARM_ARCH < 7))
  asm volatile("yield");
#elif FOLLY_PPC64
  asm volatile("or 27,27,27");
#endif
}

/*
 * A helper object for the contended case. Starts off with eager
 * spinning, and falls back to sleeping for small quantums.
 */
static constexpr std::chrono::nanoseconds kMinYieldingSleep =
    std::chrono::microseconds(500);
    
class Sleeper {
  const std::chrono::nanoseconds delta;
  uint32_t spinCount;

  static constexpr uint32_t kMaxActiveSpin = 4000;

 public:

  constexpr Sleeper() noexcept : delta(kMinYieldingSleep), spinCount(0) {}

  explicit Sleeper(std::chrono::nanoseconds d) noexcept
      : delta(d), spinCount(0) {}

  void wait() noexcept {
    if (spinCount < kMaxActiveSpin) {
      ++spinCount;
      asm_volatile_pause();
    } else {
      /* sleep override */
      std::this_thread::sleep_for(delta);
    }
  }
};

} // namespace detail
} // namespace skiplist
} // namespace utility
