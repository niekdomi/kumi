/// @file error.hpp
/// @brief Error types for parsing and lexing
///
/// @see Lexer for tokenization errors
/// @see Parser for parsing errors

#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace kumi {

/// @brief Represents a parsing error with position and optional hint
struct ParseError final
{
    std::string message;  ///< Error message
    std::size_t position; ///< Position in source where error occurred
    std::string hint;     ///< Optional hint for fixing the error
};

/// @brief Creates a ParseError wrapped in std::unexpected
/// @tparam T Expected return type
/// @param message Error message
/// @param position Position in source where error occurred
/// @param hint Optional hint for fixing the error
/// @return Unexpected ParseError
template<typename T>
[[nodiscard]]
auto error(std::string_view message, std::size_t position, std::string_view hint = "")
  -> std::expected<T, ParseError>
{
    return std::unexpected(ParseError{
      .message = std::string(message), .position = position, .hint = std::string(hint) });
}

} // namespace kumi
