/// @file colors.hpp
/// @brief ANSI color codes and utilities for terminal output

#pragma once

#include <format>
#include <string>
#include <string_view>

namespace kumi::color {

// Reset and modifiers
constexpr std::string_view RESET = "\033[0m";
constexpr std::string_view BOLD = "\033[1m";
constexpr std::string_view DIM = "\033[2m";
constexpr std::string_view ITALIC = "\033[3m";
constexpr std::string_view UNDERLINE = "\033[4m";

// Foreground colors
constexpr std::string_view BLACK = "\033[30m";
constexpr std::string_view RED = "\033[31m";
constexpr std::string_view GREEN = "\033[32m";
constexpr std::string_view YELLOW = "\033[33m";
constexpr std::string_view BLUE = "\033[34m";
constexpr std::string_view MAGENTA = "\033[35m";
constexpr std::string_view CYAN = "\033[36m";
constexpr std::string_view WHITE = "\033[37m";

// Bright foreground colors
constexpr std::string_view BRIGHT_BLACK = "\033[90m";
constexpr std::string_view BRIGHT_RED = "\033[91m";
constexpr std::string_view BRIGHT_GREEN = "\033[92m";
constexpr std::string_view BRIGHT_YELLOW = "\033[93m";
constexpr std::string_view BRIGHT_BLUE = "\033[94m";
constexpr std::string_view BRIGHT_MAGENTA = "\033[95m";
constexpr std::string_view BRIGHT_CYAN = "\033[96m";
constexpr std::string_view BRIGHT_WHITE = "\033[97m";

// Background colors
constexpr std::string_view BG_BLACK = "\033[40m";
constexpr std::string_view BG_RED = "\033[41m";
constexpr std::string_view BG_GREEN = "\033[42m";
constexpr std::string_view BG_YELLOW = "\033[43m";
constexpr std::string_view BG_BLUE = "\033[44m";
constexpr std::string_view BG_MAGENTA = "\033[45m";
constexpr std::string_view BG_CYAN = "\033[46m";
constexpr std::string_view BG_WHITE = "\033[47m";

// Bright background colors
constexpr std::string_view BG_BRIGHT_BLACK = "\033[100m";
constexpr std::string_view BG_BRIGHT_RED = "\033[101m";
constexpr std::string_view BG_BRIGHT_GREEN = "\033[102m";
constexpr std::string_view BG_BRIGHT_YELLOW = "\033[103m";
constexpr std::string_view BG_BRIGHT_BLUE = "\033[104m";
constexpr std::string_view BG_BRIGHT_MAGENTA = "\033[105m";
constexpr std::string_view BG_BRIGHT_CYAN = "\033[106m";
constexpr std::string_view BG_BRIGHT_WHITE = "\033[107m";

// Semantic color aliases
constexpr std::string_view SUCCESS = GREEN;
constexpr std::string_view ERROR = RED;
constexpr std::string_view WARNING = YELLOW;
constexpr std::string_view INFO = CYAN;

// Utility functions
[[nodiscard]]
auto rgb(int r, int g, int b) -> std::string
{
    return std::format("\033[38;2;{};{};{}m", r, g, b);
}

[[nodiscard]]
inline auto bg_rgb(int r, int g, int b) -> std::string
{
    return std::format("\033[48;2;{};{};{}m", r, g, b);
}

[[nodiscard]]
inline auto color_256(int code) -> std::string
{
    return std::format("\033[38;5;{}m", code);
}

[[nodiscard]]
inline auto bg_color_256(int code) -> std::string
{
    return std::format("\033[48;5;{}m", code);
}

[[nodiscard]]
inline auto colorize(std::string_view text, std::string_view color) -> std::string
{
    return std::format("{}{}{}", color, text, RESET);
}

[[nodiscard]]
inline auto bold(std::string_view text) -> std::string
{
    return std::format("{}{}{}", BOLD, text, RESET);
}

[[nodiscard]]
inline auto dim(std::string_view text) -> std::string
{
    return std::format("{}{}{}", DIM, text, RESET);
}

} // namespace kumi::color
