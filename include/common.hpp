// -------------------------------------------------------------------------------------
//
// docs: https://github.com/ErenKarakas1/cpputils/blob/main/docs/common.md
// src: https://github.com/ErenKarakas1/cpputils
// license: MIT
//
// -------------------------------------------------------------------------------------

#ifndef UTILS_COMMON_HPP
#define UTILS_COMMON_HPP

#include <cstdint>

#ifndef NDEBUG
#include <cstdlib>
#include <print>
#include <source_location>
#endif

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

#define UNUSED(x) (void)(x)

#ifndef NDEBUG
[[noreturn]] constexpr void TODO(const char* message = nullptr, const std::source_location loc = std::source_location::current()) {
    if (message == nullptr) {
        std::println(stderr, "TODO at [{}:{}]", loc.file_name(), loc.line());
    } else {
        std::println(stderr, "TODO at [{}:{}]: {}", loc.file_name(), loc.line(), message);
    }
    std::abort();
}
#else
#define TODO(...) (void)0
#endif

#ifndef NDEBUG
constexpr void ASSERT(const bool condition, const char* message = nullptr, const std::source_location loc = std::source_location::current()) {
    if (!condition) {
        if (message == nullptr) {
            std::println(stderr, "Assert failed at [{}:{}]", loc.file_name(), loc.line());
        } else {
            std::println(stderr, "Assert failed at [{}:{}]: {}", loc.file_name(), loc.line(), message);
        }
        std::abort();
    }
}
#else
#define ASSERT(...) (void)0
#endif

#ifndef NDEBUG
[[noreturn]] constexpr void UNREACHABLE(const char* message = nullptr) {
    UNUSED(message);
#if defined(_MSC_VER) && !defined(__clang__)
    __assume(false);
#else
    __builtin_unreachable();
#endif
}
#else
#define UNREACHABLE(...) (void)0
#endif

// https://www.foonathan.net/2020/09/move-forward/
#define MOVE(...) static_cast<std::remove_reference_t<decltype(__VA_ARGS__)>&&>(__VA_ARGS__)
#define FORWARD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

#endif // UTILS_COMMON_HPP
