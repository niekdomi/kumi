/// @file raw_mode.hpp
/// @brief Terminal raw mode configuration with blocking I/O
///
/// Provides RAII-style terminal mode management that configures the terminal
/// for raw input with blocking reads, eliminating the need for polling loops.

#pragma once

#include "ui/core/ansi.hpp"

#include <print>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>

namespace kumi::ui {

/// @brief RAII wrapper for terminal raw mode configuration
///
/// Configures the terminal for raw input mode with blocking I/O:
/// - Disables echo, canonical mode, and signal handling
/// - Disables flow control and input processing
/// - Uses blocking reads (VMIN=1, VTIME=0) for efficient event-driven input
/// - Hides cursor during operation
///
/// Example usage:
/// ```cpp
/// {
///     RawMode raw_mode;
///     auto event = wait_for_input(); // Blocks efficiently until input
/// }
/// // Terminal automatically restored on scope exit
/// ```
class RawMode final
{
  public:
    /// @brief Enables raw mode and configures terminal for blocking I/O
    /// @throws std::runtime_error if terminal configuration fails
    RawMode()
    {
        if (tcgetattr(STDIN_FILENO, &original_termios_) == -1) {
            throw std::runtime_error("Failed to get terminal attributes");
        }

        struct termios raw = original_termios_;

        // Disable echo, canonical mode, and signal handling
        raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | ISIG);

        // Disable flow control and CR-to-NL translation
        raw.c_iflag &= ~static_cast<tcflag_t>(IXON | ICRNL);

        // Configure for blocking reads:
        // VMIN = 1: read() blocks until at least 1 byte is available
        // VTIME = 0: no timeout, pure blocking behavior
        //
        // This eliminates the need for polling loops - the kernel will
        // efficiently suspend the process until input arrives, consuming
        // zero CPU while waiting.
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
            throw std::runtime_error("Failed to set terminal to raw mode");
        }

        std::print("{}", ansi::CURSOR_HIDE);
        static_cast<void>(std::fflush(stdout));
    }

    /// @brief Restores original terminal settings and shows cursor
    ~RawMode()
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios_);
        std::print("{}", ansi::CURSOR_SHOW);
        static_cast<void>(std::fflush(stdout));
    }

    RawMode(const RawMode&) = delete;
    auto operator=(const RawMode&) -> RawMode& = delete;
    RawMode(RawMode&&) = delete;
    auto operator=(RawMode&&) -> RawMode& = delete;

  private:
    struct termios original_termios_{}; ///< Original terminal settings
};

} // namespace kumi::ui
