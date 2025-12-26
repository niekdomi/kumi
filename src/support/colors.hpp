/// @file colors.hpp
/// @brief ANSI color codes for terminal output
///
/// Provides a unified color namespace for diagnostics, CLI, and TUI components.

#pragma once

#include <string_view>

namespace kumi::color {

// Reset and modifiers
constexpr std::string_view RESET = "\033[0m";
constexpr std::string_view BOLD = "\033[1m";
constexpr std::string_view DIM = "\033[2m";

// Standard colors
constexpr std::string_view BLACK = "\033[30m";
constexpr std::string_view RED = "\033[31m";
constexpr std::string_view GREEN = "\033[32m";
constexpr std::string_view YELLOW = "\033[33m";
constexpr std::string_view BLUE = "\033[34m";
constexpr std::string_view MAGENTA = "\033[35m";
constexpr std::string_view CYAN = "\033[36m";
constexpr std::string_view WHITE = "\033[37m";

// Bright colors
constexpr std::string_view BRIGHT_BLACK = "\033[90m";
constexpr std::string_view BRIGHT_RED = "\033[91m";
constexpr std::string_view BRIGHT_GREEN = "\033[92m";
constexpr std::string_view BRIGHT_YELLOW = "\033[93m";
constexpr std::string_view BRIGHT_BLUE = "\033[94m";
constexpr std::string_view BRIGHT_MAGENTA = "\033[95m";
constexpr std::string_view BRIGHT_CYAN = "\033[96m";
constexpr std::string_view BRIGHT_WHITE = "\033[97m";

} // namespace kumi::color
