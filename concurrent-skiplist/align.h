#pragma once

#include <cstddef>
#include <cstdint>

namespace utility {
namespace skiplist {


#if defined(__arm__)
#define FOLLY_ARM 1
#else
#define FOLLY_ARM 0
#endif

#if defined(__s390x__)
#define FOLLY_S390X 1
#else
#define FOLLY_S390X 0
#endif


constexpr bool kIsArchArm = FOLLY_ARM == 1;
constexpr bool kIsArchS390X = FOLLY_S390X == 1;

namespace detail {

// Implemented this way because of a bug in Clang for ARMv7, which gives the
// wrong result for `alignof` a `union` with a field of each scalar type.


// 优化上面的代码 使其支持c++11编译

template<std::size_t Align, typename... Rest>
struct max_align_helper;

template<std::size_t Align>
struct max_align_helper<Align> {
    static constexpr std::size_t value = Align;
};


template<std::size_t Align, typename T, typename... Rest>
struct max_align_helper<Align, T, Rest...> {
    static constexpr std::size_t value = max_align_helper<
        (Align > alignof(T) ? Align : alignof(T)),
        Rest...
    >::value;
};

template <typename... Ts>
struct max_align_t_ {
    static constexpr std::size_t value = max_align_helper<0, Ts...>::value;
};

using max_align_v_ = max_align_t_<
    long double,
    double,
    float,
    long long int,
    long int,
    int,
    short int,
    bool,
    char,
    char16_t,
    char32_t,
    wchar_t,
    void*,
    std::max_align_t
>;

} // namespace detail

constexpr std::size_t max_align_v = detail::max_align_v_::value;
struct alignas(max_align_v) max_align_t {};

constexpr std::size_t hardware_destructive_interference_size =
    (kIsArchArm || kIsArchS390X) ? 64 : 128;
static_assert(hardware_destructive_interference_size >= max_align_v, "math?");

} // namespace skiplist
} // namespace utility