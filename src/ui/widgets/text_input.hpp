/// @file text_input.hpp
/// @brief Text input widget for terminal UI
///
/// Provides an interactive text input field with placeholder support,
/// keyboard navigation, and word deletion capabilities.
///
/// @see Select for single-selection menus
/// @see MultiSelect for multi-selection menus

#pragma once

#include "cli/colors.hpp"
#include "ui/core/ansi.hpp"
#include "ui/core/input.hpp"
#include "ui/widgets/common/terminal_state.hpp"

#include <cstdlib>
#include <print>
#include <string>
#include <string_view>

namespace kumi::ui {

/// @brief Interactive text input widget
///
/// Displays a prompt and allows the user to enter text with support for:
/// - Backspace for character deletion
/// - Ctrl+Backspace for word deletion
/// - Ctrl+C to exit program
/// - Enter to submit
/// - Placeholder text when empty
///
/// Example usage:
/// ```cpp
/// TextInput input{"Enter name", "John Doe"};
/// auto result = input.run();
/// ```
class TextInput final
{
  public:
    /// @brief Constructs a text input widget
    /// @param prompt Prompt text to display
    /// @param placeholder Default value shown when input is empty
    /// @param term_state Terminal capabilities (auto-detected if not provided)
    explicit TextInput(std::string_view prompt,
                       std::string placeholder = "",
                       TerminalState term_state = TerminalState{})
        : prompt_(prompt),
          placeholder_(std::move(placeholder)),
          term_state_(term_state)
    {}

    /// @brief Runs the input widget and waits for user input
    /// @return User's input, or placeholder if input was empty
    [[nodiscard]]
    auto run() -> std::string
    {
        render();

        while (true) {
            const auto event = wait_for_input();

            switch (event.key) {
                case Key::ENTER: std::println(""); return value_.empty() ? placeholder_ : value_;

                case Key::BACKSPACE:
                    if (!value_.empty()) {
                        value_.pop_back();
                        render();
                    }
                    break;

                case Key::CTRL_BACKSPACE:
                    delete_last_word();
                    render();
                    break;

                case Key::CTRL_C: std::println(""); std::exit(0);

                case Key::PRINTABLE:
                    value_ += event.character;
                    render();
                    break;

                default: break;
            }
        }
    }

    /// @brief Gets the current input value
    /// @return Current input string
    [[nodiscard]]
    auto value() const noexcept -> std::string_view
    {
        return value_;
    }

    /// @brief Sets the input value programmatically
    /// @param new_value New value to set
    auto set_value(std::string_view new_value) -> void
    {
        value_ = new_value;
        render();
    }

    /// @brief Clears the input value
    auto clear() noexcept -> void
    {
        value_.clear();
        render();
    }

    /// @brief Checks if the input is empty
    /// @return true if input is empty, false otherwise
    [[nodiscard]]
    auto empty() const noexcept -> bool
    {
        return value_.empty();
    }

    /// @brief Gets the placeholder text
    /// @return Placeholder string
    [[nodiscard]]
    auto placeholder() const noexcept -> std::string_view
    {
        return placeholder_;
    }

  private:
    std::string prompt_;
    std::string placeholder_;
    std::string value_;
    TerminalState term_state_;

    //===------------------------------------------------------------------===//
    // Rendering
    //===------------------------------------------------------------------===//

    /// @brief Renders the current state of the input widget
    auto render() const -> void
    {
        std::print("\r{}", ansi::CLEAR_LINE);
        std::print(
          "{}{}{}: ", term_state_.color(color::BOLD), prompt_, term_state_.color(color::RESET));

        if (value_.empty() && !placeholder_.empty()) {
            std::print("{}{}{}",
                       term_state_.color(color::DIM),
                       placeholder_,
                       term_state_.color(color::RESET));
        } else if (!value_.empty()) {
            std::print("{}", value_);
        }

        static_cast<void>(std::fflush(stdout));
    }

    //===------------------------------------------------------------------===//
    // Input Processing
    //===------------------------------------------------------------------===//

    /// @brief Deletes the last word from the input
    ///
    /// Removes characters from the end until a word boundary is found.
    /// Whitespace-only input is cleared entirely.
    auto delete_last_word() noexcept -> void
    {
        if (value_.empty()) {
            return;
        }

        // Find the last non-whitespace character
        const auto pos = value_.find_last_not_of(" \t");
        if (pos == std::string::npos) {
            value_.clear();
            return;
        }

        // Find the start of the last word
        const auto word_start = value_.find_last_of(" \t", pos);
        value_.erase(word_start == std::string::npos ? 0 : word_start + 1);
    }
};

} // namespace kumi::ui
