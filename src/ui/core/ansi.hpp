/// @file ansi.hpp
/// @brief ANSI escape sequences for terminal control

#pragma once

#include <format>
#include <string>
#include <string_view>

namespace kumi::ui::ansi {

// Cursor control
constexpr std::string_view CURSOR_HIDE = "\033[?25l";
constexpr std::string_view CURSOR_SHOW = "\033[?25h";
constexpr std::string_view CURSOR_SAVE = "\033[s";
constexpr std::string_view CURSOR_RESTORE = "\033[u";

// Screen control
constexpr std::string_view CLEAR_SCREEN = "\033[2J";
constexpr std::string_view CLEAR_LINE = "\033[2K";
constexpr std::string_view CLEAR_LINE_FROM_CURSOR = "\033[K";

// Color codes (re-exported from color namespace for convenience)
constexpr std::string_view RESET = "\033[0m";
constexpr std::string_view BOLD = "\033[1m";
constexpr std::string_view DIM = "\033[2m";

constexpr std::string_view RED = "\033[31m";
constexpr std::string_view GREEN = "\033[32m";
constexpr std::string_view YELLOW = "\033[33m";
constexpr std::string_view BLUE = "\033[34m";
constexpr std::string_view CYAN = "\033[36m";

[[nodiscard]]
inline auto move_up(int n = 1) -> std::string
{
    return std::format("\033[{}A", n);
}

[[nodiscard]]
inline auto move_down(int n = 1) -> std::string
{
    return std::format("\033[{}B", n);
}

[[nodiscard]]
inline auto move_right(int n = 1) -> std::string
{
    return std::format("\033[{}C", n);
}

[[nodiscard]]
inline auto move_left(int n = 1) -> std::string
{
    return std::format("\033[{}D", n);
}

[[nodiscard]]
inline auto move_to(int row, int col) -> std::string
{
    return std::format("\033[{};{}H", row, col);
}

[[nodiscard]]
inline auto move_to_column(int col) -> std::string
{
    return std::format("\033[{}G", col);
}

} // namespace kumi::ui::ansi
