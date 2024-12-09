#pragma once

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <iostream>

namespace utility {
namespace skiplist {

inline void sizedFree(void* ptr, size_t size) {
    free(ptr);
}


template <typename T>
class SysAllocator {
 private:
  using Self = SysAllocator<T>;

 public:
  using value_type = T;

  constexpr SysAllocator() = default;

  constexpr SysAllocator(SysAllocator const&) = default;

  template <typename U, typename std::enable_if<!std::is_same<U, T>::value, bool>::type* = nullptr>

  constexpr SysAllocator(SysAllocator<U> const&) noexcept {}

  T* allocate(size_t count) {
    auto const p = std::malloc(sizeof(T) * count);
    if (!p) {
    //   throw_exception<std::bad_alloc>();
      throw std::bad_alloc();
    }
    return static_cast<T*>(p);
  }
  void deallocate(T* p, size_t count) { sizedFree(p, count * sizeof(T)); }

  friend bool operator==(Self const&, Self const&) noexcept { return true; }
  friend bool operator!=(Self const&, Self const&) noexcept { return false; }
};

template <typename T, class Inner, bool FallbackToStdAlloc = false>
class CxxAllocatorAdaptor : private std::allocator<T> {
 private:
  using Self = CxxAllocatorAdaptor<T, Inner, FallbackToStdAlloc>;

  template <typename U, typename UInner, bool UFallback>
  friend class CxxAllocatorAdaptor;

  Inner* inner_ = nullptr;

 public:
  using value_type = T;

  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  template <bool X = FallbackToStdAlloc, typename std::enable_if<X, bool>::type* = nullptr>
  constexpr explicit CxxAllocatorAdaptor() {}

  constexpr explicit CxxAllocatorAdaptor(Inner& ref) : inner_(&ref) {}

  constexpr CxxAllocatorAdaptor(CxxAllocatorAdaptor const&) = default;

  template <typename U, typename std::enable_if<!std::is_same<U, T>::value, bool>::type* = nullptr>
  constexpr CxxAllocatorAdaptor(
      CxxAllocatorAdaptor<U, Inner, FallbackToStdAlloc> const& other)
      : inner_(other.inner_) {}

  CxxAllocatorAdaptor& operator=(CxxAllocatorAdaptor const& other) = default;

  template <typename U, typename std::enable_if<!std::is_same<U, T>::value, bool>::type* = nullptr>
  CxxAllocatorAdaptor& operator=(
      CxxAllocatorAdaptor<U, Inner, FallbackToStdAlloc> const& other) noexcept {
    inner_ = other.inner_;
    return *this;
  }

  T* allocate(std::size_t n) {
    if (FallbackToStdAlloc && inner_ == nullptr) {
      return std::allocator<T>::allocate(n);
    }
    return static_cast<T*>(inner_->allocate(sizeof(T) * n));
  }

  void deallocate(T* p, std::size_t n) {
    if (inner_ != nullptr) {
      inner_->deallocate(p, sizeof(T) * n);
    } else {
      assert(FallbackToStdAlloc);
      std::allocator<T>::deallocate(p, n);
    }
  }

  friend bool operator==(Self const& a, Self const& b) noexcept {
    return a.inner_ == b.inner_;
  }
  friend bool operator!=(Self const& a, Self const& b) noexcept {
    return a.inner_ != b.inner_;
  }

  template <typename U>
  struct rebind {
    using other = CxxAllocatorAdaptor<U, Inner, FallbackToStdAlloc>;
  };
};


/**
 * AllocatorHasTrivialDeallocate
 *
 * Unambiguously inherits std::integral_constant<bool, V> for some bool V.
 *
 * Describes whether a C++ Aallocator has trivial, i.e. no-op, deallocate().
 *
 * Also may be used to describe types which may be used with
 * CxxAllocatorAdaptor.
 */
template <typename Alloc>
struct AllocatorHasTrivialDeallocate : std::false_type {};


template <typename T, class Alloc>
struct AllocatorHasTrivialDeallocate<CxxAllocatorAdaptor<T, Alloc>>
    : AllocatorHasTrivialDeallocate<Alloc> {};





// Traits.h
//  Lighter-weight than Conjunction, but evaluates all sub-conditions eagerly.

template <bool... Bs>
struct Bools {
  using valid_type = bool;
  static constexpr std::size_t size() { return sizeof...(Bs); }
};

template <class... Ts>
struct StrictConjunction
    : std::is_same<Bools<Ts::value...>, Bools<(Ts::value || true)...>> {};


} // namespace skiplist
} // namespace utility