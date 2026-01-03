/// @file multi_select.hpp
/// @brief Multi-selection menu widget for terminal UI
///
/// Provides an interactive menu where users can select multiple options
/// using arrow keys, vim-style navigation (j/k), and spacebar to toggle.
///
/// @see Select for single-selection menus
/// @see TextInput for text input widgets

#pragma once

#include "cli/colors.hpp"
#include "ui/core/ansi.hpp"
#include "ui/core/input.hpp"
#include "ui/core/terminal_utils.hpp"
#include "ui/widgets/common/symbols.hpp"
#include "ui/widgets/common/terminal_state.hpp"

#include <cstddef>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kumi::ui {

/// @brief Interactive multi-selection menu widget
///
/// Displays a list of options with check-boxes that can be toggled.
/// Supports keyboard navigation and returns all selected items.
///
/// Navigation:
/// - Arrow Up/Down or k/j: Move cursor
/// - Space: Toggle selection
/// - Enter: Confirm selection
/// - Ctrl+C: Exit program
///
/// Example usage:
/// ```cpp
/// MultiSelect menu{"Choose features", {"Option A", "Option B", "Option C"}};
/// auto selected = menu.run();
/// ```
class MultiSelect final
{
  public:
    /// @brief Constructs a multi-select menu widget
    /// @param prompt Prompt text displayed above options
    /// @param options List of selectable options
    /// @param term_state Terminal capabilities (auto-detected if not provided)
    explicit MultiSelect(std::string_view prompt,
                         std::vector<std::string> options,
                         TerminalState term_state = TerminalState{})
        : prompt_(prompt),
          options_(std::move(options)),
          selected_(options_.size(), false),
          term_state_(term_state)
    {}

    /// @brief Runs the menu and waits for user selection
    /// @return Vector of selected option strings
    [[nodiscard]]
    auto run() -> std::vector<std::string>
    {
        render();

        while (true) {
            const auto event = wait_for_input();

            switch (event.key) {
                case Key::ARROW_UP:
                    if (current_index_ > 0) {
                        --current_index_;
                        render();
                    }
                    break;
                case Key::ARROW_DOWN:
                    if (current_index_ < options_.size() - 1) {
                        ++current_index_;
                        render();
                    }
                    break;
                case Key::PRINTABLE: handle_input(event.character); break;
                case Key::ENTER:     std::println(""); return get_selected_options();
                case Key::CTRL_C:    std::println(""); std::exit(0);

                default: break;
            }
        }
    }

    /// @brief Gets currently selected options without exiting
    /// @return Vector of selected option strings
    [[nodiscard]]
    auto get_selected_options() const -> std::vector<std::string>
    {
        std::vector<std::string> result;

        for (std::size_t i = 0; i < options_.size(); ++i) {
            if (selected_[i]) {
                result.push_back(options_[i]);
            }
        }

        return result;
    }

    /// @brief Gets the current cursor position
    /// @return Zero-based index of currently highlighted option
    [[nodiscard]]
    auto current_index() const noexcept -> std::size_t
    {
        return current_index_;
    }

    /// @brief Checks if a specific option is selected
    /// @param index Zero-based option index
    /// @return true if option is selected, false otherwise
    [[nodiscard]]
    auto is_selected(std::size_t index) const noexcept -> bool
    {
        return index < selected_.size() && selected_[index];
    }

  private:
    std::string prompt_;
    std::vector<std::string> options_;
    std::vector<bool> selected_;
    std::size_t current_index_{0};
    TerminalState term_state_;
    bool rendered_once_{false};

    //===------------------------------------------------------------------===//
    // Input Handling
    //===------------------------------------------------------------------===//

    /// @brief Processes printable character input
    /// @param c Character to process
    auto handle_input(char c) -> void
    {
        if (c == ' ') {
            selected_[current_index_] = !selected_[current_index_];
            render();
        } else if (c == 'k' && current_index_ > 0) {
            --current_index_;
            render();
        } else if (c == 'j' && current_index_ < options_.size() - 1) {
            ++current_index_;
            render();
        }
    }

    //===------------------------------------------------------------------===//
    // Rendering
    //===------------------------------------------------------------------===//

    /// @brief Renders the current menu state to the terminal
    auto render() -> void
    {
        if (rendered_once_) {
            terminal::move_cursor_up(static_cast<int>(options_.size()) + 1);
        }

        std::print("\r{}", ansi::CLEAR_LINE);
        std::println(
          "{}{}{}:", term_state_.color(color::BOLD), prompt_, term_state_.color(color::RESET));

        for (std::size_t i = 0; i < options_.size(); ++i) {
            std::print("\r{}", ansi::CLEAR_LINE);

            const std::string_view checkbox =
              selected_[i] ? symbols::CHECKBOX_CHECKED : symbols::CHECKBOX_UNCHECKED;

            if (i == current_index_) {
                std::println("  {}{}{} {}{}",
                             term_state_.color(color::CYAN),
                             term_state_.color(color::BOLD),
                             checkbox,
                             options_[i],
                             term_state_.color(color::RESET));
            } else {
                std::println("  {}{} {}{}",
                             term_state_.color(color::DIM),
                             checkbox,
                             options_[i],
                             term_state_.color(color::RESET));
            }
        }

        static_cast<void>(std::fflush(stdout));
        rendered_once_ = true;
    }
};

} // namespace kumi::ui
