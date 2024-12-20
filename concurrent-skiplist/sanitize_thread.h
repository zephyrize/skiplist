#pragma once


#include <cstddef>

namespace utility {
namespace skiplist {

constexpr bool kIsSanitizeThread = false;


enum class annotate_rwlock_level : long {
  rdlock = 0,
  wrlock = 1,
};

namespace detail {

using annotate_rwlock_cd_t = void(char const*, int, void const volatile*);
using annotate_rwlock_ar_t = void(char const*, int, void const volatile*, long);
using annotate_benign_race_sized_t =
    void(char const*, int, void const volatile*, long, char const*);
using annotate_ignore_t = void(char const*, int);

extern annotate_rwlock_cd_t* const annotate_rwlock_create_v;
extern annotate_rwlock_cd_t* const annotate_rwlock_create_static_v;
extern annotate_rwlock_cd_t* const annotate_rwlock_destroy_v;
extern annotate_rwlock_ar_t* const annotate_rwlock_acquired_v;
extern annotate_rwlock_ar_t* const annotate_rwlock_released_v;
extern annotate_benign_race_sized_t* const annotate_benign_race_sized_v;
extern annotate_ignore_t* const annotate_ignore_reads_begin_v;
extern annotate_ignore_t* const annotate_ignore_reads_end_v;
extern annotate_ignore_t* const annotate_ignore_writes_begin_v;
extern annotate_ignore_t* const annotate_ignore_writes_end_v;
extern annotate_ignore_t* const annotate_ignore_sync_begin_v;
extern annotate_ignore_t* const annotate_ignore_sync_end_v;

} // namespace detail


// always inline
#ifdef _MSC_VER
#define FOLLY_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__)
#define FOLLY_ALWAYS_INLINE inline __attribute__((__always_inline__))
#else
#define FOLLY_ALWAYS_INLINE inline
#endif


FOLLY_ALWAYS_INLINE static void annotate_rwlock_create(
    void const volatile* const addr, char const* const f, int const l) {
  auto fun = detail::annotate_rwlock_create_v;
  return kIsSanitizeThread && fun ? fun(f, l, addr) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_rwlock_create_static(
    void const volatile* const addr, char const* const f, int const l) {
  auto fun = detail::annotate_rwlock_create_static_v;
  return kIsSanitizeThread && fun ? fun(f, l, addr) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_rwlock_destroy(
    void const volatile* const addr, char const* const f, int const l) {
  auto fun = detail::annotate_rwlock_destroy_v;
  return kIsSanitizeThread && fun ? fun(f, l, addr) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_rwlock_acquired(
    void const volatile* const addr,
    annotate_rwlock_level const w,
    char const* const f,
    int const l) {
  auto fun = detail::annotate_rwlock_acquired_v;
  return kIsSanitizeThread && fun ? fun(f, l, addr, long(w)) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_rwlock_try_acquired(
    void const volatile* const addr,
    annotate_rwlock_level const w,
    bool const result,
    char const* const f,
    int const l) {
  return result ? annotate_rwlock_acquired(addr, w, f, l) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_rwlock_released(
    void const volatile* const addr,
    annotate_rwlock_level const w,
    char const* const f,
    int const l) {
  auto fun = detail::annotate_rwlock_released_v;
  return kIsSanitizeThread && fun ? fun(f, l, addr, long(w)) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_benign_race_sized(
    void const volatile* const addr,
    long const size,
    char const* const desc,
    char const* const f,
    int const l) {
  auto fun = detail::annotate_benign_race_sized_v;
  return kIsSanitizeThread && fun ? fun(f, l, addr, size, desc) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_ignore_reads_begin(
    char const* const f, int const l) {
  auto fun = detail::annotate_ignore_reads_begin_v;
  return kIsSanitizeThread && fun ? fun(f, l) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_ignore_reads_end(
    char const* const f, int const l) {
  auto fun = detail::annotate_ignore_reads_end_v;
  return kIsSanitizeThread && fun ? fun(f, l) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_ignore_writes_begin(
    char const* const f, int const l) {
  auto fun = detail::annotate_ignore_writes_begin_v;
  return kIsSanitizeThread && fun ? fun(f, l) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_ignore_writes_end(
    char const* const f, int const l) {
  auto fun = detail::annotate_ignore_writes_end_v;
  return kIsSanitizeThread && fun ? fun(f, l) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_ignore_sync_begin(
    char const* const f, int const l) {
  auto fun = detail::annotate_ignore_sync_begin_v;
  return kIsSanitizeThread && fun ? fun(f, l) : void();
}

FOLLY_ALWAYS_INLINE static void annotate_ignore_sync_end(
    char const* const f, int const l) {
  auto fun = detail::annotate_ignore_sync_end_v;
  return kIsSanitizeThread && fun ? fun(f, l) : void();
}

class annotate_ignore_thread_sanitizer_guard {
 public:
  annotate_ignore_thread_sanitizer_guard(
      char const* const file, int const line) noexcept
      : file_{file}, line_{line} {
    annotate_ignore_reads_begin(file_, line_);
    annotate_ignore_writes_begin(file_, line_);
  }

  annotate_ignore_thread_sanitizer_guard(
      const annotate_ignore_thread_sanitizer_guard&) = delete;
  annotate_ignore_thread_sanitizer_guard& operator=(
      const annotate_ignore_thread_sanitizer_guard&) = delete;

  ~annotate_ignore_thread_sanitizer_guard() {
    annotate_ignore_reads_end(file_, line_);
    annotate_ignore_writes_end(file_, line_);
  }

 private:
  char const* const file_;
  int const line_;
};


} // namespace skiplist
} // namespace utility
