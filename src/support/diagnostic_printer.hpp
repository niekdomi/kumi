/// @file diagnostic_printer.hpp
/// @brief Beautiful error message formatting with source context
///
/// Provides Rust/Clang-style error messages with source snippets,
/// line numbers, and visual indicators.

#pragma once

#include "cli/colors.hpp"
#include "support/parse_error.hpp"

#include <format>
#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace kumi {

/// @brief Formats and prints beautiful error messages with source context
class DiagnosticPrinter final
{
  public:
    /// @brief Constructs a diagnostic printer
    /// @param source Original source code
    /// @param filename Name of the file being parsed
    explicit DiagnosticPrinter(std::string_view source, std::string_view filename = "build.kumi")
        : source_(source),
          filename_(filename)
    {
        // Split source into lines for quick lookup
        std::string_view remaining = source_;
        while (!remaining.empty()) {
            auto pos = remaining.find('\n');
            if (pos == std::string_view::npos) {
                lines_.emplace_back(remaining);
                break;
            }
            lines_.emplace_back(remaining.substr(0, pos));
            remaining = remaining.substr(pos + 1);
        }
    }

    /// @brief Prints a beautiful error message
    /// @param error The parse error to display
    /// @param hint Optional hint or suggestion
    auto print_error(const ParseError &error, std::string_view hint = "") const -> void
    {
        std::println(std::cerr,
                     "{}{}error: {}{}{}",
                     color::BOLD,
                     color::RED,
                     color::RESET,
                     color::BOLD,
                     error.message);

        // Lazy coordinate conversion: only compute line/column when printing
        const auto [line, column] = position_to_line_column(error.position, source_);
        print_location(line, column);
        print_source_snippet(line, column, hint);

        std::println(std::cerr);
    }

  private:
    std::string_view source_;
    std::string_view filename_;
    std::vector<std::string_view> lines_;

    /// @brief Converts a position offset to (line, column) coordinates
    /// @param offset Position in the input string
    /// @param input The input string (for validation)
    /// @return Pair of (line, column) starting from (1, 1)
    [[nodiscard]]
    static auto position_to_line_column(std::size_t offset, std::string_view input) noexcept
      -> std::pair<std::size_t, std::size_t>
    {
        std::size_t line = 1;
        std::size_t last_newline_pos = 0;

        // Scan up to the error position
        for (std::size_t i = 0; i < offset && i < input.size(); ++i) {
            if (input[i] == '\n') {
                ++line;
                last_newline_pos = i;
            }
        }

        const std::size_t column = offset - last_newline_pos;
        return {line, column};
    }

    /// @brief Prints the file location (e.g., "--> build.kumi:5:3")
    auto print_location(std::size_t line, std::size_t column) const -> void
    {
        std::print(std::cerr, "{}  --> ", color::BLUE);
        std::print(std::cerr, "{}{}:{}:{}", color::BOLD, filename_, line, column);
        std::println(std::cerr, "{}", color::RESET);
    }

    /// @brief Prints the source snippet with visual indicator
    auto print_source_snippet(std::size_t line, std::size_t column, std::string_view hint) const
      -> void
    {
        if (line == 0 || line > lines_.size()) {
            return;
        }

        const auto line_idx = line - 1;
        const auto &line_text = lines_[line_idx];

        // Calculate gutter width (for line numbers)
        const auto gutter_width = std::to_string(line).length();

        // Print separator
        std::println(
          std::cerr, "{}{}|{}", color::BLUE, std::string(gutter_width + 1, ' '), color::RESET);

        // Print the source line
        std::println(
          std::cerr, "{}{:>{}} |{} {}", color::BLUE, line, gutter_width, color::RESET, line_text);

        // Print the indicator (^^^)
        std::print(
          std::cerr, "{}{}|{} ", color::BLUE, std::string(gutter_width + 1, ' '), color::RESET);

        // Add spacing before the indicator
        if (column > 1) {
            std::print(std::cerr, "{}", std::string(column - 1, ' '));
        }

        // Print the caret indicator with optional hint inline
        if (!hint.empty()) {
            std::println(std::cerr, "{}{}^ {}{}", color::BOLD, color::RED, hint, color::RESET);

            // Show next line for context if available (skip blank lines)
            std::size_t next_line_idx =
              line; // line is 1-indexed, so lines_[line] is the (line+1)-th line
            // Skip blank lines to show meaningful context
            while (next_line_idx < lines_.size() && lines_[next_line_idx].empty()) {
                next_line_idx++;
            }

            if (next_line_idx < lines_.size()) {
                const auto next_line_num = next_line_idx + 1;
                const auto next_gutter_width = std::to_string(next_line_num).length();
                std::println(std::cerr,
                             "{}{:>{}} |{} {}",
                             color::BLUE,
                             next_line_num,
                             next_gutter_width,
                             color::RESET,
                             lines_[next_line_idx]);
            }
        } else {
            std::println(std::cerr, "{}{}^{}", color::BOLD, color::RED, color::RESET);
        }
    }
};

} // namespace kumi
