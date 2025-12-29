#pragma once

#include "cli/tui/core/ansi.hpp"

#include <print>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>

namespace kumi {

class RawMode
{
  public:
    RawMode()
    {
        if (tcgetattr(STDIN_FILENO, &original_termios_) == -1) {
            throw std::runtime_error("Failed to get terminal attributes");
        }

        struct termios raw = original_termios_;
        raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | ISIG);
        raw.c_iflag &= ~static_cast<tcflag_t>(IXON | ICRNL);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 1;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
            throw std::runtime_error("Failed to set terminal to raw mode");
        }

        std::print("{}", ansi::CURSOR_HIDE);
        static_cast<void>(std::fflush(stdout));
    }

    ~RawMode()
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios_);
        std::print("{}", ansi::CURSOR_SHOW);
        static_cast<void>(std::fflush(stdout));
    }

    RawMode(const RawMode &) = delete;
    auto operator=(const RawMode &) -> RawMode & = delete;
    RawMode(RawMode &&) = delete;
    auto operator=(RawMode &&) -> RawMode & = delete;

  private:
    struct termios original_termios_{};
};

} // namespace kumi
