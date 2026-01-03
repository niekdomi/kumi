/// @file select.hpp
/// @brief Single-selection menu widget for terminal UI
///
/// Provides an interactive menu where users can select one option from a list
/// using arrow keys or vim-style navigation (j/k).
///
/// @see MultiSelect for multi-selection menus
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

/// @brief Interactive single-selection menu widget
///
/// Displays a list of options with radio buttons where only one can be selected.
/// Supports keyboard navigation and returns the selected item.
///
/// Navigation:
/// - Arrow Up/Down or k/j: Move cursor
/// - Enter: Confirm selection
/// - Ctrl+C: Exit program
///
/// Example usage:
/// ```cpp
/// Select menu{"Choose build type", {"Debug", "Release", "RelWithDebInfo"}, 1};
/// auto selected = menu.run();
/// ```
class Select final
{
  public:
    /// @brief Constructs a single-select menu widget
    /// @param prompt Prompt text displayed above options
    /// @param options List of selectable options
    /// @param default_index Initial selection index (defaults to 0)
    /// @param term_state Terminal capabilities (auto-detected if not provided)
    explicit Select(std::string_view prompt,
                    std::vector<std::string> options,
                    std::size_t default_index = 0,
                    TerminalState term_state = TerminalState{})
        : prompt_(prompt),
          options_(std::move(options)),
          selected_index_(std::clamp(default_index, std::size_t{0}, options_.size() - 1)),
          term_state_(term_state)
    {}

    /// @brief Runs the menu and waits for user selection
    /// @return Selected option string
    [[nodiscard]]
    auto run() -> std::string
    {
        render();

        while (true) {
            const auto event = wait_for_input();

            switch (event.key) {
                case Key::ARROW_UP:
                    if (selected_index_ > 0) {
                        --selected_index_;
                        render();
                    }
                    break;
                case Key::ARROW_DOWN:
                    if (selected_index_ < options_.size() - 1) {
                        ++selected_index_;
                        render();
                    }
                    break;
                case Key::PRINTABLE: handle_vim_navigation(event.character); break;
                case Key::ENTER:     std::println(""); return options_[selected_index_];
                case Key::CTRL_C:    std::println(""); std::exit(0);
                default:             break;
            }
        }
    }

    /// @brief Gets the currently selected option index
    /// @return Zero-based index of selected option
    [[nodiscard]]
    auto selected_index() const noexcept -> std::size_t
    {
        return selected_index_;
    }

    /// @brief Gets the currently selected option text
    /// @return Selected option string
    [[nodiscard]]
    auto selected_option() const noexcept -> std::string_view
    {
        return options_[selected_index_];
    }

    /// @brief Sets the selected index programmatically
    /// @param index New selection index
    auto set_selected_index(std::size_t index) -> void
    {
        if (index < options_.size()) {
            selected_index_ = index;
            render();
        }
    }

  private:
    std::string prompt_;
    std::vector<std::string> options_;
    std::size_t selected_index_;
    TerminalState term_state_;
    bool rendered_once_{false};

    //===------------------------------------------------------------------===//
    // Input Handling
    //===------------------------------------------------------------------===//

    /// @brief Processes vim-style navigation keys
    /// @param c Character to process
    auto handle_vim_navigation(char c) -> void
    {
        if (c == 'k' && selected_index_ > 0) {
            --selected_index_;
            render();
        } else if (c == 'j' && selected_index_ < options_.size() - 1) {
            ++selected_index_;
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

            if (i == selected_index_) {
                std::println("  {}{}{} {}{}",
                             term_state_.color(color::CYAN),
                             term_state_.color(color::BOLD),
                             symbols::RADIO_SELECTED,
                             options_[i],
                             term_state_.color(color::RESET));
            } else {
                std::println("  {}{} {}{}",
                             term_state_.color(color::DIM),
                             symbols::RADIO_UNSELECTED,
                             options_[i],
                             term_state_.color(color::RESET));
            }
        }

        static_cast<void>(std::fflush(stdout));
        rendered_once_ = true;
    }
};

} // namespace kumi::ui
