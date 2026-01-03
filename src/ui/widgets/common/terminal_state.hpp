/// @file terminal_state.hpp
/// @brief Terminal capability detection and state management
///
/// Provides utilities for detecting terminal capabilities (TTY, color support)
/// to enable graceful degradation in different environments.

#pragma once

#include <cstdlib>
#include <string_view>
#include <unistd.h>

namespace kumi::ui {

/// @brief Detects if stdout is connected to a TTY
/// @return true if stdout is a terminal, false otherwise
[[nodiscard]]
inline auto detect_tty() noexcept -> bool
{
    return ::isatty(STDOUT_FILENO) != 0;
}

/// @brief Detects if color output should be enabled
/// @return true if colors should be used, false otherwise
///
/// Colors are disabled if:
/// - Not connected to a TTY
/// - NO_COLOR environment variable is set
[[nodiscard]]
inline auto detect_color_enabled() noexcept -> bool
{
    return detect_tty() && (std::getenv("NO_COLOR") == nullptr);
}

/// @brief Terminal capability state
///
/// Encapsulates terminal capabilities for reuse across widgets.
/// Typically constructed once and passed to widgets.
struct TerminalState final
{
    bool is_tty{detect_tty()};
    bool color_enabled{detect_color_enabled()};

    /// @brief Constructs terminal state with automatic detection
    TerminalState() noexcept = default;

    /// @brief Constructs terminal state with explicit values
    /// @param tty Whether output is to a TTY
    /// @param color Whether color output is enabled
    constexpr TerminalState(bool tty, bool color) noexcept : is_tty(tty), color_enabled(color) {}

    /// @brief Returns color code if enabled, empty string otherwise
    /// @param color_code ANSI color code to conditionally apply
    /// @return The color code if colors are enabled, empty string otherwise
    [[nodiscard]]
    constexpr auto color(std::string_view color_code) const noexcept -> std::string_view
    {
        return color_enabled ? color_code : "";
    }
};

} // namespace kumi::ui
