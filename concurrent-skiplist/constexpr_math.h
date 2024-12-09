#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
namespace utility {
namespace skiplist {


// 旧编译器无法通过
// template <typename T, typename Less>
// constexpr T const& constexpr_clamp(
//     T const& v, T const& lo, T const& hi, Less less) {
//   T const& a = less(v, lo) ? lo : v;
//   T const& b = less(hi, a) ? hi : a;
//   return b;
// }

template <typename T, typename Less>
constexpr T const& constexpr_clamp(T const& v, T const& lo, T const& hi, Less less) {
  return less(hi, less(v, lo) ? lo : v) ? hi : (less(v, lo) ? lo : v);
}

template <typename T>
constexpr T const& constexpr_clamp(T const& v, T const& lo, T const& hi) {
  return constexpr_clamp(v, lo, hi, std::less<T>{});
}

// TLDR: Prefer using operator< for ordering. And when
// a and b are equivalent objects, we return b to make
// sorting stable.
// See http://stepanovpapers.com/notes.pdf for details.


template<typename T>
constexpr T constexpr_max(T a) {
    return a;
}
template<typename T, typename... Ts>
constexpr T constexpr_max(T a, Ts... ts) {
    return constexpr_max(std::max(a, constexpr_max(ts...)));
}

// When a and b are equivalent objects, we return a to
// make sorting stable.

template<typename T>
constexpr T constexpr_min(T a) {
    return a;
}

template<typename T, typename... Ts>
constexpr T constexpr_min(T a, Ts... ts) {
    return constexpr_min(std::min(a, constexpr_min(ts...)));
}

template <typename Dst, typename Src>
constexpr typename std::enable_if<std::is_integral<Src>::value, Dst>::type
constexpr_clamp_cast(Src src) {
  static_assert(
      std::is_integral<Dst>::value && sizeof(Dst) <= sizeof(int64_t),
      "constexpr_clamp_cast can only cast into integral type (up to 64bit)");

  using L = std::numeric_limits<Dst>;
  // clang-format off
  return
    // Check if Src and Dst have same signedness.
    std::is_signed<Src>::value == std::is_signed<Dst>::value
    ? (
      // Src and Dst have same signedness. If sizeof(Src) <= sizeof(Dst),
      // we can safely convert Src to Dst without any loss of accuracy.
      sizeof(Src) <= sizeof(Dst) ? Dst(src) :
      // If Src is larger in size, we need to clamp it to valid range in Dst.
      Dst(constexpr_clamp(src, Src(L::min()), Src(L::max()))))
    // Src and Dst have different signedness.
    // Check if it's signed -> unsigend cast.
    : std::is_signed<Src>::value && std::is_unsigned<Dst>::value
    ? (
      // If src < 0, the result should be 0.
      src < 0 ? Dst(0) :
      // Otherwise, src >= 0. If src can fit into Dst, we can safely cast it
      // without loss of accuracy.
      sizeof(Src) <= sizeof(Dst) ? Dst(src) :
      // If Src is larger in size than Dst, we need to ensure the result is
      // at most Dst MAX.
      Dst(constexpr_min(src, Src(L::max()))))
    // It's unsigned -> signed cast.
    : (
      // Since Src is unsigned, and Dst is signed, Src can fit into Dst only
      // when sizeof(Src) < sizeof(Dst).
      sizeof(Src) < sizeof(Dst) ? Dst(src) :
      // If Src does not fit into Dst, we need to ensure the result is at most
      // Dst MAX.
      Dst(constexpr_min(src, Src(L::max()))));
  // clang-format on
}


namespace detail {
// Upper/lower bound values that could be accurately represented in both
// integral and float point types.
constexpr double kClampCastLowerBoundDoubleToInt64F = -9223372036854774784.0;
constexpr double kClampCastUpperBoundDoubleToInt64F = 9223372036854774784.0;
constexpr double kClampCastUpperBoundDoubleToUInt64F = 18446744073709549568.0;

constexpr float kClampCastLowerBoundFloatToInt32F = -2147483520.0f;
constexpr float kClampCastUpperBoundFloatToInt32F = 2147483520.0f;
constexpr float kClampCastUpperBoundFloatToUInt32F = 4294967040.0f;

// This works the same as constexpr_clamp, but the comparison are done in Src
// to prevent any implicit promotions.
template <typename D, typename S>
constexpr D constexpr_clamp_cast_helper(S src, S sl, S su, D dl, D du) {
  return src < sl ? dl : (src > su ? du : D(src));
}
} // namespace detail

template <typename T>
constexpr bool constexpr_isnan(T const t) {
  return t != t; // NOLINT
}

template <typename Dst, typename Src>
constexpr typename std::enable_if<std::is_floating_point<Src>::value, Dst>::type
constexpr_clamp_cast(Src src) {
  static_assert(
      std::is_integral<Dst>::value && sizeof(Dst) <= sizeof(int64_t),
      "constexpr_clamp_cast can only cast into integral type (up to 64bit)");

  using L = std::numeric_limits<Dst>;
  // clang-format off
  return
    // Special case: cast NaN into 0.
    constexpr_isnan(src) ? Dst(0) :
    // using `sizeof(Src) > sizeof(Dst)` as a heuristic that Dst can be
    // represented in Src without loss of accuracy.
    // see: https://en.wikipedia.org/wiki/Floating-point_arithmetic
    sizeof(Src) > sizeof(Dst) ?
      detail::constexpr_clamp_cast_helper(
          src, Src(L::min()), Src(L::max()), L::min(), L::max()) :
    // sizeof(Src) < sizeof(Dst) only happens when doing cast of
    // 32bit float -> u/int64_t.
    // Losslessly promote float into double, change into double -> u/int64_t.
    sizeof(Src) < sizeof(Dst) ? (
      src >= 0.0
      ? constexpr_clamp_cast<Dst>(
            constexpr_clamp_cast<std::uint64_t>(double(src)))
      : constexpr_clamp_cast<Dst>(
            constexpr_clamp_cast<std::int64_t>(double(src)))) :
    // The following are for sizeof(Src) == sizeof(Dst).
    std::is_same<Src, double>::value && std::is_same<Dst, int64_t>::value ?
      detail::constexpr_clamp_cast_helper(
          double(src),
          detail::kClampCastLowerBoundDoubleToInt64F,
          detail::kClampCastUpperBoundDoubleToInt64F,
          L::min(),
          L::max()) :
    std::is_same<Src, double>::value && std::is_same<Dst, uint64_t>::value ?
      detail::constexpr_clamp_cast_helper(
          double(src),
          0.0,
          detail::kClampCastUpperBoundDoubleToUInt64F,
          L::min(),
          L::max()) :
    std::is_same<Src, float>::value && std::is_same<Dst, int32_t>::value ?
      detail::constexpr_clamp_cast_helper(
          float(src),
          detail::kClampCastLowerBoundFloatToInt32F,
          detail::kClampCastUpperBoundFloatToInt32F,
          L::min(),
          L::max()) :
      detail::constexpr_clamp_cast_helper(
          float(src),
          0.0f,
          detail::kClampCastUpperBoundFloatToUInt32F,
          L::min(),
          L::max());
  // clang-format on
}


} // namespace skiplist
} // namespace utility