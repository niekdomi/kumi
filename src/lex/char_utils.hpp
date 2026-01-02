/// @file char_utils.hpp
/// @brief Fast character classification using lookup tables
///
/// Provides O(1) character classification functions using a precomputed
/// 256-entry lookup table with bit flags for different character classes.

#pragma once

#include <array>
#include <cstdint>
#include <ranges>
#include <utility>

namespace kumi {

enum class Char : std::uint8_t
{
    DIGIT = 1U << 0U,
    ALPHA = 1U << 1U,
    IDENT = 1U << 2U,
    SPACE = 1U << 3U,
};

namespace detail {

[[nodiscard]]
constexpr auto compute_map() noexcept -> std::array<std::uint8_t, 256>
{
    std::array<std::uint8_t, 256> table{};

    const auto set = [&](unsigned char c, Char flag) -> void {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index))
        table[c] |= std::to_underlying(flag);
    };

    for (const auto i : std::views::iota(0, 256)) {
        const auto c = static_cast<unsigned char>(i);

        if (c >= '0' && c <= '9') {
            set(c, Char::DIGIT);
            set(c, Char::IDENT);
        }

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            set(c, Char::ALPHA);
            set(c, Char::IDENT);
        }

        if (c == '_' || c == '-') {
            set(c, Char::IDENT);
        }

        if (c == ' ' || (c >= '\t' && c <= '\r')) {
            set(c, Char::SPACE);
        }
    }

    return table;
}

// Computed at compile-time, stored in the data segment.
constexpr auto LOOKUP_TABLE = compute_map();

} // namespace detail

/**
 * @brief Core classification logic.
 * Optimized to a single memory load and a bitwise AND.
 */
[[nodiscard]]
constexpr auto is_type(char c, Char flag) noexcept -> bool
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index))
    return (detail::LOOKUP_TABLE[static_cast<std::uint8_t>(c)] & std::to_underlying(flag)) != 0;
}

[[nodiscard]]
constexpr auto is_space(char c) noexcept -> bool
{
    return is_type(c, Char::SPACE);
}

[[nodiscard]]
constexpr auto is_digit(char c) noexcept -> bool
{
    return is_type(c, Char::DIGIT);
}

[[nodiscard]]
constexpr auto is_alpha(char c) noexcept -> bool
{
    return is_type(c, Char::ALPHA);
}

[[nodiscard]]
constexpr auto is_identifier(char c) noexcept -> bool
{
    return is_type(c, Char::IDENT);
}

} // namespace kumi
