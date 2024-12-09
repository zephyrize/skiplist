#include "sanitize_thread.h"
#include "extern.h"

// abseil uses size_t for size params while other FB code and libraries use
// long, so it is helpful to keep these declarations out of widely-included
// header files.

// extern "C" void AnnotateRWLockCreate(
//     char const* f, int l, const volatile void* addr);

// extern "C" void AnnotateRWLockCreateStatic(
//     char const* f, int l, const volatile void* addr);

// extern "C" void AnnotateRWLockDestroy(
//     char const* f, int l, const volatile void* addr);

extern "C" void AnnotateRWLockAcquired(
    char const* f, int l, const volatile void* addr, long w);

extern "C" void AnnotateRWLockReleased(
    char const* f, int l, const volatile void* addr, long w);

// extern "C" void AnnotateBenignRaceSized(
//     char const* f,
//     int l,
//     const volatile void* addr,
//     long size,
//     char const* desc);

// extern "C" void AnnotateIgnoreReadsBegin(char const* f, int l);

// extern "C" void AnnotateIgnoreReadsEnd(char const* f, int l);

// extern "C" void AnnotateIgnoreWritesBegin(char const* f, int l);

// extern "C" void AnnotateIgnoreWritesEnd(char const* f, int l);

// extern "C" void AnnotateIgnoreSyncBegin(char const* f, int l);

// extern "C" void AnnotateIgnoreSyncEnd(char const* f, int l);

namespace {

FOLLY_CREATE_EXTERN_ACCESSOR(
annotate_rwlock_acquired_access_v, AnnotateRWLockAcquired);

FOLLY_CREATE_EXTERN_ACCESSOR(
    annotate_rwlock_released_access_v, AnnotateRWLockReleased);

#if 0
FOLLY_CREATE_EXTERN_ACCESSOR(
    annotate_rwlock_create_access_v, AnnotateRWLockCreate);

FOLLY_CREATE_EXTERN_ACCESSOR(
    annotate_rwlock_create_static_access_v, AnnotateRWLockCreateStatic);

FOLLY_CREATE_EXTERN_ACCESSOR(
    annotate_rwlock_destroy_access_v, AnnotateRWLockDestroy);

FOLLY_CREATE_EXTERN_ACCESSOR(
    annotate_benign_race_sized_access_v, AnnotateBenignRaceSized);

FOLLY_CREATE_EXTERN_ACCESSOR(
    annotate_ignore_reads_begin_access_v, AnnotateIgnoreReadsBegin);

FOLLY_CREATE_EXTERN_ACCESSOR(
    annotate_ignore_reads_end_access_v, AnnotateIgnoreReadsEnd);

FOLLY_CREATE_EXTERN_ACCESSOR(
    annotate_ignore_writes_begin_access_v, AnnotateIgnoreWritesBegin);

FOLLY_CREATE_EXTERN_ACCESSOR(
    annotate_ignore_writes_end_access_v, AnnotateIgnoreWritesEnd);

FOLLY_CREATE_EXTERN_ACCESSOR(
    annotate_ignore_sync_begin_access_v, AnnotateIgnoreSyncBegin);

FOLLY_CREATE_EXTERN_ACCESSOR(
    annotate_ignore_sync_end_access_v, AnnotateIgnoreSyncEnd);
    
#endif
} // namespace


namespace utility {
namespace skiplist {
namespace detail {
    
static constexpr auto const E = kIsSanitizeThread;

// #ifdef _MSC_VER
// #define FOLLY_STORAGE_CONSTEXPR
// #else
// #define FOLLY_STORAGE_CONSTEXPR constexpr
// #endif

annotate_rwlock_ar_t* const annotate_rwlock_acquired_v =
    annotate_rwlock_acquired_access_v<E>;

annotate_rwlock_ar_t* const annotate_rwlock_released_v =
    annotate_rwlock_released_access_v<E>;

#if 0
FOLLY_STORAGE_CONSTEXPR annotate_rwlock_cd_t* const annotate_rwlock_create_v =
    annotate_rwlock_create_access_v<E>;

FOLLY_STORAGE_CONSTEXPR annotate_rwlock_cd_t* const
    annotate_rwlock_create_static_v = annotate_rwlock_create_static_access_v<E>;

FOLLY_STORAGE_CONSTEXPR annotate_rwlock_cd_t* const annotate_rwlock_destroy_v =
    annotate_rwlock_destroy_access_v<E>;

FOLLY_STORAGE_CONSTEXPR annotate_rwlock_ar_t* const annotate_rwlock_acquired_v =
    annotate_rwlock_acquired_access_v<E>;

FOLLY_STORAGE_CONSTEXPR annotate_rwlock_ar_t* const annotate_rwlock_released_v =
    annotate_rwlock_released_access_v<E>;

FOLLY_STORAGE_CONSTEXPR annotate_benign_race_sized_t* const
    annotate_benign_race_sized_v = annotate_benign_race_sized_access_v<E>;

FOLLY_STORAGE_CONSTEXPR annotate_ignore_t* const annotate_ignore_reads_begin_v =
    annotate_ignore_reads_begin_access_v<E>;

FOLLY_STORAGE_CONSTEXPR annotate_ignore_t* const annotate_ignore_reads_end_v =
    annotate_ignore_reads_end_access_v<E>;

FOLLY_STORAGE_CONSTEXPR annotate_ignore_t* const
    annotate_ignore_writes_begin_v = annotate_ignore_writes_begin_access_v<E>;

FOLLY_STORAGE_CONSTEXPR annotate_ignore_t* const annotate_ignore_writes_end_v =
    annotate_ignore_writes_end_access_v<E>;

FOLLY_STORAGE_CONSTEXPR annotate_ignore_t* const annotate_ignore_sync_begin_v =
    annotate_ignore_sync_begin_access_v<E>;

FOLLY_STORAGE_CONSTEXPR annotate_ignore_t* const annotate_ignore_sync_end_v =
    annotate_ignore_sync_end_access_v<E>;
#endif 
} // namespace detail
} // namespace skiplist
} // namespace utility
