/// @file terminal_state.hpp
/// @brief Terminal capability detection and state management
///
/// Provides thread-safe utilities for detecting terminal capabilities
/// (TTY, color support) to enable graceful degradation in different environments.

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

/// @brief Detects if color output should be enabled (thread-safe, cached)
/// @return true if colors should be used, false otherwise
///
/// Colors are disabled if:
/// - Not connected to a TTY
/// - NO_COLOR environment variable is set
///
/// The result is cached on first call for thread-safe access.
[[nodiscard]]
inline auto detect_color_enabled() noexcept -> bool
{
    static const bool cached = []() noexcept -> bool {
        if (!detect_tty()) {
            return false;
        }
        // NOTE: Called once during initialization before any threads are spawned
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        return std::getenv("NO_COLOR") == nullptr;
    }();
    return cached;
}

/// @brief Terminal capability state
///
/// Encapsulates terminal capabilities for reuse across widgets.
/// Construct once at program startup and pass to widgets.
///
/// Example:
/// ```cpp
/// TerminalState term_state; // Detect capabilities
/// Spinner spinner{"Loading", term_state};
/// ```
struct TerminalState final
{
    /// @brief Constructs terminal state with automatic detection
    TerminalState() noexcept = default;

    /// @brief Constructs terminal state with explicit values
    /// @param tty Whether output is to a TTY
    /// @param color Whether color output is enabled
    constexpr TerminalState(bool tty, bool color) noexcept : is_tty(tty), color_enabled(color) {}

    bool is_tty{detect_tty()};
    bool color_enabled{detect_color_enabled()};

    /// @brief Returns color code if enabled, empty string otherwise
    /// @param color_code ANSI color code to conditionally apply
    /// @return The color code if colors are enabled, empty string otherwise
    ///
    /// Usage:
    /// ```cpp
    /// std::println("{}{}{} Success",
    ///     term_state.color(color::GREEN),
    ///     symbol,
    ///     term_state.color(color::RESET));
    /// ```
    [[nodiscard]]
    constexpr auto color(std::string_view color_code) const noexcept -> std::string_view
    {
        return color_enabled ? color_code : "";
    }
};

} // namespace kumi::ui
