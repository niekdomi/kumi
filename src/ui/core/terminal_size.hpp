#pragma once

#include <expected>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

namespace kumi {

struct TerminalSize
{
    int rows;
    int cols;
};

[[nodiscard]]
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

} // namespace kumi
