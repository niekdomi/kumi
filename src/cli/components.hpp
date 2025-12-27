#pragma once

#include "cli/input.hpp"
#include "cli/terminal.hpp"
#include "support/colors.hpp"

#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace kumi {

class TextInput
{
  public:
    explicit TextInput(std::string_view prompt, std::string placeholder = "") :
      prompt_(prompt),
      placeholder_(std::move(placeholder))
    {
    }

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

  private:
    void render()
    {
        std::print("\r{}", ansi::CLEAR_LINE);
        std::print("{}{}{}: ", color::BOLD, prompt_, color::RESET);

        if (value_.empty() && !placeholder_.empty()) {
            std::print("{}{}{}", color::DIM, placeholder_, color::RESET);
        } else if (!value_.empty()) {
            std::print("{}", value_);
        }

        static_cast<void>(std::fflush(stdout));
    }

    void delete_last_word()
    {
        if (value_.empty())
            return;

        const auto pos = value_.find_last_not_of(" \t");
        if (pos == std::string::npos) {
            value_.clear();
            return;
        }

        const auto word_start = value_.find_last_of(" \t", pos);
        value_.erase(word_start == std::string::npos ? 0 : word_start + 1);
    }

    std::string prompt_;
    std::string placeholder_;
    std::string value_;
};

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
                std::println(
                  "  {}{}{}{} {}", color::CYAN, color::BOLD, "●", color::RESET, options_[i]);
            } else {
                std::println("  {}{}{} {}", color::DIM, "○", color::RESET, options_[i]);
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

class MultiSelect
{
  public:
    MultiSelect(std::string_view prompt, std::vector<std::string> options) :
      prompt_(prompt),
      options_(std::move(options)),
      selected_(options_.size(), false)
    {
    }

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
        std::println("{}{}{}:", color::BOLD, prompt_, color::RESET);

        for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
            std::print("\r{}", ansi::CLEAR_LINE);

            const auto *checkbox = selected_[i] ? "☑" : "☐";

            if (i == current_index_) {
                std::println(
                  "  {}{}{}{} {}", color::CYAN, color::BOLD, checkbox, color::RESET, options_[i]);
            } else {
                std::println("  {}{}{} {}", color::DIM, checkbox, color::RESET, options_[i]);
            }
        }

        static_cast<void>(std::fflush(stdout));
        rendered_once_ = true;
    }

    std::string prompt_;
    std::vector<std::string> options_;
    std::vector<bool> selected_;
    int current_index_{ 0 };
    bool rendered_once_{ false };
};

} // namespace kumi
