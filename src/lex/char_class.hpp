/// @file char_class.hpp
/// @brief Fast character classification using lookup tables
///
/// Provides O(1) character classification functions using a precomputed
/// 256-entry lookup table with bit flags for different character classes.

#pragma once

#include <array>
#include <cstdint>

namespace kumi {

/// @brief Character class bit flags
enum CharClass : uint8_t
{
    NONE = 0U,
    DIGIT = 1U << 0U, ///< '0'-'9'
    ALPHA = 1U << 1U, ///< 'a'-'z', 'A'-'Z'
    IDENT = 1U << 2U, ///< Valid in identifiers (alphanumeric + '_' + '-')
    SPACE = 1U << 3U, ///< Whitespace characters
};

/// @brief Precomputed character classification lookup table
static constexpr std::array<uint8_t, 256> CHAR_MAP = []() {
    std::array<uint8_t, 256> table{};

    for (unsigned int i = 0U; i < 256U; ++i) {
        // Digits: 0-9
        if (i >= static_cast<unsigned int>('0') && i <= static_cast<unsigned int>('9')) {
            table[i] |= static_cast<uint8_t>(DIGIT | IDENT);
        }

        // Letters: a-z, A-Z
        if ((i >= static_cast<unsigned int>('a') && i <= static_cast<unsigned int>('z'))
            || (i >= static_cast<unsigned int>('A') && i <= static_cast<unsigned int>('Z'))) {
            table[i] |= static_cast<uint8_t>(ALPHA | IDENT);
        }

        // Identifier special chars: _ -
        if (i == static_cast<unsigned int>('_') || i == static_cast<unsigned int>('-')) {
            table[i] |= static_cast<uint8_t>(IDENT);
        }

        // Whitespace: space, tab, newline, etc.
        if (i == ' ' || i == '\t' || i == '\n' || i == '\v' || i == '\f' || i == '\r') {
            table[i] |= static_cast<uint8_t>(SPACE);
        }
    }

    return table;
}();

/// @brief Checks if character is whitespace
/// @param c Character to check
/// @return true if whitespace, false otherwise
[[nodiscard]]
inline auto is_space(char c) noexcept -> bool
{
    const auto index = static_cast<uint8_t>(c);
    return (CHAR_MAP[index] & SPACE) != 0U;
}

/// @brief Checks if character is a digit (0-9)
/// @param c Character to check
/// @return true if digit, false otherwise
[[nodiscard]]
inline auto is_digit(char c) noexcept -> bool
{
    const auto index = static_cast<uint8_t>(c);
    return (CHAR_MAP[index] & DIGIT) != 0U;
}

/// @brief Checks if character is alphabetic (a-z, A-Z)
/// @param c Character to check
/// @return true if alphabetic, false otherwise
[[nodiscard]]
inline auto is_alpha(char c) noexcept -> bool
{
    const auto index = static_cast<uint8_t>(c);
    return (CHAR_MAP[index] & ALPHA) != 0U;
}

/// @brief Checks if character is valid in an identifier
/// @param c Character to check
/// @return true if valid in identifier, false otherwise
[[nodiscard]]
inline auto is_ident(char c) noexcept -> bool
{
    const auto index = static_cast<uint8_t>(c);
    return (CHAR_MAP[index] & IDENT) != 0U;
}

} // namespace kumi
