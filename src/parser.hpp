#pragma once

#include "ast.hpp"
#include "error.hpp"
#include "token.hpp"

#include <expected>
#include <span>

namespace kumi {

class Parser final
{
  public:
    explicit Parser(std::span<const Token> tokens) : tokens_(tokens) {}

    [[nodiscard]]
    auto parse() -> std::expected<AST, ParseError>
    {
        return std::unexpected(ParseError{
          .message = "not implemented",
          .line = 0,
          .column = 0,
        });
    }

  private:
    std::span<const Token> tokens_;
    std::size_t line_{ 1 };
    std::size_t column_{ 1 };
    std::size_t position_{ 0 };
};

} // namespace kumi
