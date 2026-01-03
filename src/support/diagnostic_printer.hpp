/// @file diagnostic_printer.hpp
/// @brief Diagnostic output with source context
///
/// Provides beautiful error messages with line numbers, source snippets,
/// and visual indicators for parse errors.

#pragma once

#include "cli/colors.hpp"
#include "support/parse_error.hpp"

#include <algorithm>
#include <format>
#include <iostream>
#include <print>
#include <ranges>
#include <string_view>
#include <utility>

namespace kumi {
// clang-format off

/// @brief Formats and displays diagnostic messages with source context
///
/// Example output:
/// ```
/// error: unexpected token '}'
///   --> build.kumi:5:3
///    │
///  5 │ target myapp {
///    │              ^ expected property or closing brace
///    │
///    = help: valid properties include 'sources', 'defines', etc.
/// ```
class DiagnosticPrinter final
{
  public:
    /// @brief Constructs a diagnostic printer
    /// @param source Complete source text
    /// @param filename Source file name for display
    explicit constexpr DiagnosticPrinter(std::string_view source,
                                         std::string_view filename) noexcept
        : source_(source),
          filename_(filename){}

    /// @brief Prints a formatted error diagnostic to stderr
    /// @param error Parse error with position and message
    auto print_error(const ParseError& error) const -> void
    {
        // `error: unexpected token '}'`
        std::println(std::cerr, "{}{}error:{}{} {}", color::BOLD, color::RED, color::RESET, color::BOLD, error.message, color::RESET);

        const auto [line, column] = position_to_coordinates(error.position);
        print_location(line, column);
        print_snippet(line, column, error.label, error.help);
    }

  private:
    std::string_view source_;
    std::string_view filename_;

    /// @brief Gets the text of a specific line
    /// @param line_num 1-indexed line number
    /// @return Line text without newline, or empty if out of bounds
    [[nodiscard]]
    constexpr auto get_line(std::size_t line_num) const noexcept -> std::string_view
    {
        if (line_num == 0) {
            return {};
        }

        auto lines = source_ | std::views::split('\n') | std::views::drop(line_num - 1);

        if (lines.empty()) {
            return {};
        }

        const auto line_range = *lines.begin();
        return {line_range.begin(), line_range.end()};
    }

    /// @brief Converts byte offset to (line, column) coordinates
    /// @param offset Byte position in source
    /// @return 1-indexed (line, column) pair
    [[nodiscard]]
    constexpr auto position_to_coordinates(std::size_t offset) const noexcept -> std::pair<std::size_t, std::size_t>
    {
        const auto prefix = source_.substr(0, std::min(offset, source_.size()));
        const auto line = std::ranges::count(prefix, '\n') + 1;

        const auto line_start = prefix.rfind('\n');
        const auto column = (line_start == std::string_view::npos) ? offset + 1 : offset - line_start;

        return {line, column};
    }

    /// @brief Prints file location line
    /// @param line Line number
    /// @param column Column number
    auto print_location(std::size_t line, std::size_t column) const -> void
    {
        // `--> build.kumi:5:3`
        std::println(std::cerr, "{}  --> {}{}:{}:{}{}", color::BLUE, color::BOLD, filename_, line, column, color::RESET);
    }

    /// @brief Prints source snippet with error indicator
    /// @param line Line number where error occurred
    /// @param column Column number where error occurred
    /// @param label Optional label for the error indicator
    /// @param help Optional help message
    auto print_snippet(std::size_t line,
                       std::size_t column,
                       std::string_view label,
                       std::string_view help) const -> void
    {
        const auto line_text = get_line(line);
        if (line_text.empty()) {
            return;
        }

        const auto gutter_width = std::format("{}", line).length();
        const auto gutter_space = std::string(gutter_width + 2, ' ');

        std::println(std::cerr, "{}{}│{}", color::BLUE, gutter_space, color::RESET);

        // `5 │ target myapp {`
        std::println(std::cerr, "{} {:>{}} │{} {}", color::BLUE, line, gutter_width, color::RESET, line_text);

        // `   │              ^ expected property or closing brace`
        std::print(std::cerr, "{}{}│{} ", color::BLUE, gutter_space, color::RESET);
        if (column > 1) {
            std::print(std::cerr, "{}", std::string(column - 1, ' '));
        }
        std::println(std::cerr, "{}{}^{}{}{}", color::BOLD, color::RED, label.empty() ? "" : " ", label, color::RESET);

        // `   = help: valid properties include 'sources', 'defines', etc.`
        if (!help.empty()) {
            std::println(std::cerr, "{}{}│{}", color::BLUE, gutter_space, color::RESET);
            std::println(std::cerr, "{}{}= {}help:{} {}", color::BLUE, gutter_space, color::BOLD, color::RESET, help);
        }
    }
};

} // namespace kumi
