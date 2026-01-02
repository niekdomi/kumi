#pragma once

#include "cli/colors.hpp"
#include "cli/tui/core/ansi.hpp"
#include "cli/tui/core/input.hpp"

#include <print>
#include <string>
#include <string_view>

namespace kumi {

class TextInput
{
  public:
    explicit TextInput(std::string_view prompt, std::string placeholder = "")
        : prompt_(prompt),
          placeholder_(std::move(placeholder))
    {}

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
        if (value_.empty()) {
            return;
        }

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

} // namespace kumi
