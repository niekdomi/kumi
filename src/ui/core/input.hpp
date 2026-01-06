/// @file input.hpp
/// @brief Terminal input handling with blocking I/O
///
/// Provides efficient event-driven keyboard input processing using blocking
/// reads instead of polling, eliminating CPU waste and improving responsiveness.
///
/// @see InputEvent for input event structure
/// @see Key for all recognized key types

#pragma once

#include <cstdint>
#include <optional>
#include <unistd.h>

namespace kumi::ui {

/// @brief Enumeration of recognized keyboard input types
enum class Key : std::uint8_t
{
    ARROW_UP,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    ENTER,
    BACKSPACE,
    CTRL_BACKSPACE,
    DELETE,
    ESCAPE,
    TAB,
    CTRL_C,
    PRINTABLE,
    UNKNOWN,
};

/// @brief Represents a keyboard input event
struct InputEvent
{
    Key key;              ///< Type of key pressed
    char character{'\0'}; ///< Character value for printable keys

    /// @brief Checks if the event represents a printable character
    /// @return true if printable, false otherwise
    [[nodiscard]]
    constexpr auto is_printable() const noexcept -> bool
    {
        return key == Key::PRINTABLE;
    }

    /// @brief Checks if the event represents an arrow key
    /// @return true if arrow key, false otherwise
    [[nodiscard]]
    constexpr auto is_arrow() const noexcept -> bool
    {
        // clang-format off
        return key == Key::ARROW_UP   ||
               key == Key::ARROW_DOWN ||
               key == Key::ARROW_LEFT ||
               key == Key::ARROW_RIGHT;
        // clang-format on
    }
};

/// @brief Reads a single input event from stdin (non-blocking)
/// @return InputEvent if available, nullopt if no input ready
///
/// This function performs a non-blocking read and immediately returns.
/// Used internally by wait_for_input() for multi-byte sequence handling.
[[nodiscard]]
inline auto read_input() -> std::optional<InputEvent>
{
    char c = 0;
    const ssize_t n = read(STDIN_FILENO, &c, 1);

    if (n <= 0) {
        return std::nullopt;
    }

    // Handle escape sequences (multi-byte)
    if (c == '\033') [[unlikely]] {
        char seq[2];

        // Read the next two bytes of the escape sequence
        if (read(STDIN_FILENO, &seq[0], 1) != 1 || read(STDIN_FILENO, &seq[1], 1) != 1) {
            return InputEvent{.key = Key::ESCAPE};
        }

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return InputEvent{.key = Key::ARROW_UP};
                case 'B': return InputEvent{.key = Key::ARROW_DOWN};
                case 'C': return InputEvent{.key = Key::ARROW_RIGHT};
                case 'D': return InputEvent{.key = Key::ARROW_LEFT};
                case '3': {
                    // Delete key sends ESC[3~
                    char tilde = 0;
                    if (read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~') {
                        return InputEvent{.key = Key::DELETE};
                    }
                    break;
                }
            }
        }

        return InputEvent{.key = Key::UNKNOWN};
    }

    // Handle special characters
    switch (c) {
        case '\r':
        case '\n': return InputEvent{.key = Key::ENTER};
        case 127:
        case '\b': return InputEvent{.key = Key::BACKSPACE};
        case 23:   return InputEvent{.key = Key::CTRL_BACKSPACE}; // Ctrl+W
        case '\t': return InputEvent{.key = Key::TAB};
        case 3:    return InputEvent{.key = Key::CTRL_C};
        default:
            // Printable ASCII characters
            if (c >= 32 && c <= 126) {
                return InputEvent{.key = Key::PRINTABLE, .character = c};
            }
            return InputEvent{.key = Key::UNKNOWN};
    }
}

/// @brief Waits for and reads the next input event (blocking)
/// @return InputEvent representing the key press
///
/// This function uses blocking I/O, which means it will efficiently wait
/// until input is available without consuming CPU cycles. The terminal
/// must be in raw mode for this to work correctly.
///
/// Benefits over polling:
/// - Zero CPU usage while waiting
/// - Immediate response when key is pressed
/// - No artificial latency from sleep delays
[[nodiscard]]
inline auto wait_for_input() -> InputEvent
{
    // This uses blocking I/O - the read() call in read_input() will block
    // until at least one byte is available. This is efficient because:
    // 1. The kernel puts the process to sleep until data arrives
    // 2. No CPU cycles are wasted polling
    // 3. Response is immediate when input is available
    //
    // Note: This requires the terminal to be in raw mode with blocking enabled,
    // which is typically set up by the terminal initialization code.
    while (true) {
        if (auto event = read_input()) [[likely]]
        {
            return *event;
        }
        // If read_input() returns nullopt, it means read() returned 0 or -1,
        // which shouldn't happen in blocking mode under normal circumstances.
        // We loop to retry in case of transient errors.
    }
}

} // namespace kumi::ui
