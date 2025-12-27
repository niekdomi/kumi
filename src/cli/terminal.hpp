/// @file terminal.hpp
/// @brief Terminal control and ANSI escape sequences
///
/// Provides utilities for terminal manipulation, cursor control,
/// and raw mode management.

#pragma once

#include <cstdio>
#include <expected>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace kumi::ansi {

// Cursor control
constexpr std::string_view CURSOR_HIDE = "\033[?25l";
constexpr std::string_view CURSOR_SHOW = "\033[?25h";
constexpr std::string_view CURSOR_SAVE = "\033[s";
constexpr std::string_view CURSOR_RESTORE = "\033[u";

// Screen control
constexpr std::string_view CLEAR_SCREEN = "\033[2J";
constexpr std::string_view CLEAR_LINE = "\033[2K";
constexpr std::string_view CLEAR_LINE_FROM_CURSOR = "\033[K";

/// @brief Moves cursor up by n lines
[[nodiscard]]
inline auto move_up(int n = 1) -> std::string
{
    return std::format("\033[{}A", n);
}

/// @brief Moves cursor down by n lines
[[nodiscard]]
inline auto move_down(int n = 1) -> std::string
{
    return std::format("\033[{}B", n);
}

/// @brief Moves cursor right by n columns
[[nodiscard]]
inline auto move_right(int n = 1) -> std::string
{
    return std::format("\033[{}C", n);
}

/// @brief Moves cursor left by n columns
[[nodiscard]]
inline auto move_left(int n = 1) -> std::string
{
    return std::format("\033[{}D", n);
}

/// @brief Moves cursor to specific row and column
[[nodiscard]]
inline auto move_to(int row, int col) -> std::string
{
    return std::format("\033[{};{}H", row, col);
}

/// @brief Moves cursor to specific column
[[nodiscard]]
inline auto move_to_column(int col) -> std::string
{
    return std::format("\033[{}G", col);
}

} // namespace kumi::ansi

namespace kumi {

// Terminal size
struct TerminalSize
{
    int rows;
    int cols;
};

// Get terminal size
inline auto get_terminal_size() -> std::expected<TerminalSize, std::string>
{
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return std::unexpected("Failed to get terminal size");
    }
    return TerminalSize{
        .rows = ws.ws_row,
        .cols = ws.ws_col,
    };
}

// RAII wrapper for terminal raw mode
// Automatically restores terminal state on destruction
class RawMode
{
  public:
    RawMode()
    {
        // Save original terminal settings
        if (tcgetattr(STDIN_FILENO, &original_termios_) == -1) {
            throw std::runtime_error("Failed to get terminal attributes");
        }

        // Set raw mode
        struct termios raw = original_termios_;
        raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | ISIG);
        raw.c_iflag &= ~static_cast<tcflag_t>(IXON | ICRNL);
        raw.c_cc[VMIN] = 0;  // Non-blocking read
        raw.c_cc[VTIME] = 1; // 100ms timeout

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
            throw std::runtime_error("Failed to set terminal to raw mode");
        }

        // Hide cursor
        std::print("{}", ansi::CURSOR_HIDE);
        static_cast<void>(std::fflush(stdout));
    }

    ~RawMode()
    {
        // Restore original terminal settings
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios_);

        // Show cursor
        std::print("{}", ansi::CURSOR_SHOW);
        static_cast<void>(std::fflush(stdout));
    }

    // Non-copyable, non-movable
    RawMode(const RawMode &) = delete;
    auto operator=(const RawMode &) -> RawMode & = delete;
    RawMode(RawMode &&) = delete;
    auto operator=(RawMode &&) -> RawMode & = delete;

  private:
    struct termios original_termios_{};
};

// Terminal utility functions
namespace terminal {
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
} // namespace terminal

} // namespace kumi
