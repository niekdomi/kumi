#pragma once

#include "cli/colors.hpp"
#include "cli/tui/core/ansi.hpp"
#include "cli/tui/core/input.hpp"
#include "cli/tui/core/terminal_utils.hpp"

#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace kumi {

namespace select_symbols {

constexpr std::string_view SELECTED = "●";
constexpr std::string_view UNSELECTED = "○";

} // namespace select_symbols

class Select
{
  public:
    Select(std::string_view prompt, std::vector<std::string> options, int default_index = 0) :
      prompt_(prompt),
      options_(std::move(options)),
      selected_index_(default_index)
    {
        if (selected_index_ < 0 || selected_index_ >= static_cast<int>(options_.size())) {
            selected_index_ = 0;
        }
    }

    [[nodiscard]]
    auto run() -> std::string
    {
        render();

        while (true) {
            const auto event = wait_for_input();

            switch (event.key) {
                case Key::ARROW_UP:
                    if (selected_index_ > 0) {
                        selected_index_--;
                        render();
                    }
                    break;

                case Key::ARROW_DOWN:
                    if (selected_index_ < static_cast<int>(options_.size()) - 1) {
                        selected_index_++;
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

    [[nodiscard]]
    auto get_selected_index() const noexcept -> int
    {
        return selected_index_;
    }

  private:
    void handle_vim_navigation(char c)
    {
        if (c == 'k' && selected_index_ > 0) {
            selected_index_--;
            render();
        } else if (c == 'j' && selected_index_ < static_cast<int>(options_.size()) - 1) {
            selected_index_++;
            render();
        }
    }

    void render()
    {
        if (rendered_once_) {
            terminal::move_cursor_up(static_cast<int>(options_.size()) + 1);
        }

        std::print("\r{}", ansi::CLEAR_LINE);
        std::println("{}{}{}:", color::BOLD, prompt_, color::RESET);

        for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
            std::print("\r{}", ansi::CLEAR_LINE);

            if (i == selected_index_) {
                std::println("  {}{}{}{} {}",
                             color::CYAN,
                             color::BOLD,
                             select_symbols::SELECTED,
                             color::RESET,
                             options_[i]);
            } else {
                std::println(
                  "  {}{}{} {}", color::DIM, select_symbols::UNSELECTED, color::RESET, options_[i]);
            }
        }

        static_cast<void>(std::fflush(stdout));
        rendered_once_ = true;
    }

    std::string prompt_;
    std::vector<std::string> options_;
    int selected_index_;
    bool rendered_once_{ false };
};

} // namespace kumi
