/// @file multi_select.hpp
/// @brief Multi-selection menu widget for terminal UI

#pragma once

#include "cli/colors.hpp"
#include "ui/core/ansi.hpp"
#include "ui/core/input.hpp"
#include "ui/core/terminal_utils.hpp"
#include "ui/widgets/common/symbols.hpp"
#include "ui/widgets/common/terminal_state.hpp"

#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace kumi::ui {

class MultiSelect
{
  public:
    MultiSelect(std::string_view prompt,
                std::vector<std::string> options,
                TerminalState term_state = TerminalState{})
        : prompt_(prompt),
          options_(std::move(options)),
          selected_(options_.size(), false),
          term_state_(term_state)
    {}

    [[nodiscard]]
    auto run() -> std::vector<std::string>
    {
        render();

        while (true) {
            const auto event = wait_for_input();

            switch (event.key) {
                case Key::ARROW_UP:
                    if (current_index_ > 0) {
                        current_index_--;
                        render();
                    }
                    break;
                case Key::ARROW_DOWN:
                    if (current_index_ < static_cast<int>(options_.size()) - 1) {
                        current_index_++;
                        render();
                    }
                    break;
                case Key::PRINTABLE: handle_input(event.character); break;
                case Key::ENTER:     std::println(""); return get_selected_options();
                case Key::CTRL_C:    std::println(""); std::exit(0);
                default:             break;
            }
        }
    }

  private:
    void handle_input(char c)
    {
        if (c == ' ') {
            selected_[current_index_] = !selected_[current_index_];
            render();
        } else if (c == 'k' && current_index_ > 0) {
            current_index_--;
            render();
        } else if (c == 'j' && current_index_ < static_cast<int>(options_.size()) - 1) {
            current_index_++;
            render();
        }
    }

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

    void render()
    {
        if (rendered_once_) {
            terminal::move_cursor_up(static_cast<int>(options_.size()) + 1);
        }

        std::print("\r{}", ansi::CLEAR_LINE);
        std::println(
          "{}{}{}{}:", term_state_.color(color::BOLD), prompt_, term_state_.color(color::RESET));

        for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
            std::print("\r{}", ansi::CLEAR_LINE);

            const std::string_view checkbox =
              selected_[i] ? symbols::CHECKBOX_CHECKED : symbols::CHECKBOX_UNCHECKED;

            if (i == current_index_) {
                std::println("  {}{}{}{}{} {}",
                             term_state_.color(color::CYAN),
                             term_state_.color(color::BOLD),
                             checkbox,
                             term_state_.color(color::RESET),
                             options_[i]);
            } else {
                std::println("  {}{}{}{} {}",
                             term_state_.color(color::DIM),
                             checkbox,
                             term_state_.color(color::RESET),
                             options_[i]);
            }
        }

        static_cast<void>(std::fflush(stdout));
        rendered_once_ = true;
    }

    std::string prompt_;
    std::vector<std::string> options_;
    std::vector<bool> selected_;
    int current_index_{0};
    TerminalState term_state_;
    bool rendered_once_{false};
};

} // namespace kumi::ui
