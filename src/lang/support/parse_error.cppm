export module kumi.lang.support;
import std;

/// @file parse_error.hpp
/// @brief Error types for parsing and lexing
///
/// @see Lexer for tokenization errors
/// @see Parser for parsing errors

export namespace kumi::lang {

/// @brief Represents a parsing error with position, inline message, and help section
struct Diagnostics final
{
    std::string_view message; ///< Main error message
    std::uint32_t position;   ///< Position in source where the indicator (`^`) will point
    std::string_view help;    ///< Detailed help or suggestion
};

/// @brief Creates a Diagnostics wrapped in std::unexpected
/// @param message Main error message
/// @param position Source position for the caret
/// @param help Suggestion or help text to show
/// @return Unexpected ParseError
[[nodiscard]]
auto error(std::string_view message, std::uint32_t position, std::string_view help = "")
  -> std::unexpected<Diagnostics>
{
    return std::unexpected(Diagnostics{
      .message = message,
      .position = position,
      .help = help,
    });
}

} // namespace kumi::lang

