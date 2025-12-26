/// @file error.hpp
/// @brief Error types for parsing and lexing
///
/// @see Lexer for tokenization errors
/// @see Parser for parsing errors

#pragma once

#include <cstddef>
#include <expected>
#include <format>
#include <string>
#include <string_view>

namespace kumi {

/// @brief Represents a parse or lex error with location information
struct ParseError
{
    std::string message; ///< Error message
    std::size_t line;    ///< Line number where error occurred
    std::size_t column;  ///< Column number where error occurred

    /// @brief Formats error as "line:column: message"
    [[nodiscard]]
    auto format() const -> std::string
    {
        return std::format("{}:{}: {}", line, column, message);
    }
};

/// @brief Creates a ParseError wrapped in std::unexpected
/// @tparam T Expected return type
/// @param message Error message
/// @param line Line number where error occurred
/// @param column Column number where error occurred
/// @return Unexpected ParseError
template<typename T>
[[nodiscard]]
auto error(std::string_view message, std::size_t line, std::size_t column)
  -> std::expected<T, ParseError>
{
    return std::unexpected(ParseError{
      .message = std::string(message),
      .line = line,
      .column = column,
    });
}

} // namespace kumi
