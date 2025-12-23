#pragma once

#include <cstddef>
#include <format>
#include <string>
namespace kumi {

struct ParseError
{
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)

    std::string message;
    std::size_t line{};
    std::size_t column{};

    // NOLINTEND(misc-non-private-member-variables-in-classes)

    [[nodiscard]]
    auto format() const -> std::string
    {
        return std::format("{}:{}: {}", line, column, message);
    }
};

} // namespace kumi
