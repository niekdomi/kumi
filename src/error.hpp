/// @file error.hpp
/// @brief Error types for parsing and lexing
///
/// @see Lexer for tokenization errors
/// @see Parser for parsing errors

#pragma once

#include <cstddef>
#include <format>
#include <string>

namespace kumi {

/// @brief Represents a parse or lex error with location information
struct ParseError
{
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)

    std::string message; ///< Error message
    std::size_t line;    ///< Line number where error occurred
    std::size_t column;  ///< Column number where error occurred

    // NOLINTEND(misc-non-private-member-variables-in-classes)

    /// @brief Formats error as "line:column: message"
    [[nodiscard]]
    auto format() const -> std::string
    {
        return std::format("{}:{}: {}", line, column, message);
    }
};

} // namespace kumi
