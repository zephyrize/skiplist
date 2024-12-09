#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <type_traits>

#include "microspinlock.h"
#include "sanitize_thread.h"
#include "sleeper.h"
#include "align.h"

namespace utility {
namespace skiplist {

/*
 * A really, *really* small spinlock for fine-grained locking of lots
 * of teeny-tiny data.
 *
 * Zero initializing these is guaranteed to be as good as calling
 * init(), since the free state is guaranteed to be all-bits zero.
 *
 * This class should be kept a POD, so we can used it in other packed
 * structs (gcc does not allow __attribute__((__packed__)) on structs that
 * contain non-POD data).  This means avoid adding a constructor, or
 * making some members private, etc.
 */
struct MicroSpinLock {
  enum { FREE = 0, LOCKED = 1 };
  // lock_ can't be std::atomic<> to preserve POD-ness.
  uint8_t lock_;

  // Initialize this MSL.  It is unnecessary to call this if you
  // zero-initialize the MicroSpinLock.
  void init() noexcept { payload()->store(FREE); }

  bool try_lock() noexcept {
    bool ret = xchg(LOCKED) == FREE;
    annotate_rwlock_try_acquired(
        this, annotate_rwlock_level::wrlock, ret, __FILE__, __LINE__);
    return ret;
  }

  void lock() noexcept {
    detail::Sleeper sleeper;
    while (xchg(LOCKED) != FREE) {
      do {
        sleeper.wait();
      } while (payload()->load(std::memory_order_relaxed) == LOCKED);
    }
    assert(payload()->load() == LOCKED);
    annotate_rwlock_acquired(
        this, annotate_rwlock_level::wrlock, __FILE__, __LINE__);
  }

  void unlock() noexcept {
    assert(payload()->load() == LOCKED);
    annotate_rwlock_released(
        this, annotate_rwlock_level::wrlock, __FILE__, __LINE__);
    payload()->store(FREE, std::memory_order_release);
  }

 private:
  std::atomic<uint8_t>* payload() noexcept {
    return reinterpret_cast<std::atomic<uint8_t>*>(&this->lock_);
  }

  uint8_t xchg(uint8_t newVal) noexcept {
    return std::atomic_exchange_explicit(
        payload(), newVal, std::memory_order_acq_rel);
  }
};
static_assert(
    std::is_standard_layout<MicroSpinLock>::value &&
        std::is_trivial<MicroSpinLock>::value,
    "MicroSpinLock must be kept a POD type.");

//////////////////////////////////////////////////////////////////////

/**
 * Array of spinlocks where each one is padded to prevent false sharing.
 * Useful for shard-based locking implementations in environments where
 * contention is unlikely.
 */


template <class T, size_t N>
struct alignas(max_align_v) SpinLockArray {
  T& operator[](size_t i) noexcept { return data_[i].lock; }

  const T& operator[](size_t i) const noexcept { return data_[i].lock; }

  constexpr size_t size() const noexcept { return N; }

 private:
  struct PaddedSpinLock {
    PaddedSpinLock() : lock() {}
    T lock;
    char padding[hardware_destructive_interference_size - sizeof(T)];
  };
  static_assert(
      sizeof(PaddedSpinLock) == hardware_destructive_interference_size,
      "Invalid size of PaddedSpinLock");

  // Check if T can theoretically cross a cache line.
  static_assert(
      max_align_v > 0 &&
          hardware_destructive_interference_size % max_align_v == 0 &&
          sizeof(T) <= max_align_v,
      "T can cross cache line boundaries");

  char padding_[hardware_destructive_interference_size];
  std::array<PaddedSpinLock, N> data_;
};

//////////////////////////////////////////////////////////////////////

typedef std::lock_guard<MicroSpinLock> MSLGuard;

//////////////////////////////////////////////////////////////////////

} // namespace skiplist
} // namespace utility
