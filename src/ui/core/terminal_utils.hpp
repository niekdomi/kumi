#pragma once

#include "ui/core/ansi.hpp"

#include <print>

namespace kumi::ui::terminal {

inline void clear_screen()
{
    std::print("{}{}", ansi::CLEAR_SCREEN, ansi::move_to(1, 1));
    static_cast<void>(std::fflush(stdout));
}

inline void clear_line()
{
    std::print("{}", ansi::CLEAR_LINE);
    static_cast<void>(std::fflush(stdout));
}

inline void move_cursor_to_column(int col)
{
    std::print("{}", ansi::move_to_column(col));
    static_cast<void>(std::fflush(stdout));
}

inline void move_cursor_up(int n = 1)
{
    std::print("{}", ansi::move_up(n));
    static_cast<void>(std::fflush(stdout));
}

inline void move_cursor_down(int n = 1)
{
    std::print("{}", ansi::move_down(n));
    static_cast<void>(std::fflush(stdout));
}

} // namespace kumi::ui::terminal
