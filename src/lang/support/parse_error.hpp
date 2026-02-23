/// @file parse_error.hpp
/// @brief Error types for parsing and lexing
///
/// @see Lexer for tokenization errors
/// @see Parser for parsing errors

#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace kumi::lang {

/// @brief Represents a parsing error with position, inline message, and help section
struct ParseError final
{
    std::string message;    ///< Main error message
    std::uint32_t position; ///< Position in source where the indicator (`^`) will point
    std::string label;      ///< Message displayed after the caret indicator
    std::string help;       ///< Detailed help or suggestion
};

/// @brief Creates a ParseError wrapped in std::unexpected
/// @tparam T Expected return type
/// @param message Main error message
/// @param position Source position for the caret
/// @param label Message to show next to the caret
/// @param help Suggestion or help text to show
/// @return Unexpected ParseError
template<typename T>
[[nodiscard]]
auto error(std::string_view message,
           std::uint32_t position,
           std::string_view label = "",
           std::string_view help = "") -> std::expected<T, ParseError>
{
    return std::unexpected(ParseError{
      .message = std::string(message),
      .position = position,
      .label = std::string(label),
      .help = std::string(help),
    });
}

} // namespace kumi::lang
