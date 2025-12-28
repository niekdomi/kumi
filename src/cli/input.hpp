#pragma once

#include <cstdint>
#include <optional>
#include <unistd.h>

namespace kumi {

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
    UNKNOWN
};

struct InputEvent
{
    Key key;
    char character{ '\0' };

    [[nodiscard]]
    constexpr auto is_printable() const noexcept -> bool
    {
        return key == Key::PRINTABLE;
    }

    [[nodiscard]]
    constexpr auto is_arrow() const noexcept -> bool
    {
        // clang-format off
        return key == Key::ARROW_UP   || key
                   == Key::ARROW_DOWN || key
                   == Key::ARROW_LEFT || key
                   == Key::ARROW_RIGHT;
        // clang-format on
    }
};

[[nodiscard]]
inline auto read_input() -> std::optional<InputEvent>
{
    char c = 0;
    const ssize_t n = read(STDIN_FILENO, &c, 1);

    if (n <= 0) {
        return std::nullopt;
    }

    // Handle escape sequences
    if (c == '\033') {
        char seq[2];

        if (read(STDIN_FILENO, &seq[0], 1) != 1 || read(STDIN_FILENO, &seq[1], 1) != 1) {
            return InputEvent{ .key = Key::ESCAPE };
        }

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return InputEvent{ .key = Key::ARROW_UP };
                case 'B': return InputEvent{ .key = Key::ARROW_DOWN };
                case 'C': return InputEvent{ .key = Key::ARROW_RIGHT };
                case 'D': return InputEvent{ .key = Key::ARROW_LEFT };
                case '3': {
                    char tilde = 0;
                    if (read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~') {
                        return InputEvent{ .key = Key::DELETE };
                    }
                    break;
                }
            }
        }

        return InputEvent{ .key = Key::UNKNOWN };
    }

    // Handle special characters
    switch (c) {
        case '\r':
        case '\n': return InputEvent{ .key = Key::ENTER };
        case 127:
        case '\b': return InputEvent{ .key = Key::BACKSPACE };
        case 23:   return InputEvent{ .key = Key::CTRL_BACKSPACE };
        case '\t': return InputEvent{ .key = Key::TAB };
        case 3:    return InputEvent{ .key = Key::CTRL_C };
        default:
            if (c >= 32 && c <= 126) {
                return InputEvent{ .key = Key::PRINTABLE, .character = c };
            }
            return InputEvent{ .key = Key::UNKNOWN };
    }
}

[[nodiscard]]
inline auto wait_for_input() -> InputEvent
{
    while (true) {
        if (auto event = read_input()) {
            return *event;
        }
        usleep(10000); // 10ms
    }
}

} // namespace kumi
