#pragma once

#include "error.hpp"
#include "token.hpp"

#include <cctype>
#include <cstddef>
#include <expected>
#include <string_view>

namespace kumi {

class Lexer final
{
  public:
    explicit Lexer(std::string_view input) : input_(input) {};

    [[nodiscard]]
    auto next_token() -> std::expected<Token, ParseError>
    {
        skip_whitespace();

        if (at_end()) {
            return Token{
                .type = TokenType::END_OF_FILE,
                .value = "",
                .line = line_,
                .column = column_,
            };
        }

        switch (peek()) {
            case '{': return parse_single_char(TokenType::LEFT_BRACE);
            case '}': return parse_single_char(TokenType::RIGHT_BRACE);
            case '[': return parse_single_char(TokenType::LEFT_BRACKET);
            case ']': return parse_single_char(TokenType::RIGHT_BRACKET);
            case '(': return parse_single_char(TokenType::LEFT_PAREN);
            case ')': return parse_single_char(TokenType::RIGHT_PAREN);
            case ':': return parse_single_char(TokenType::COLON);
            case ';': return parse_single_char(TokenType::SEMICOLON);
            case ',': return parse_single_char(TokenType::COMMA);
            case '?': return parse_single_char(TokenType::QUESTION);
            case '$': return parse_single_char(TokenType::DOLLAR);
            case '.': return parse_dot();
            case '!': return parse_bang();
            case '=': return parse_equal();
            case '<': return parse_less();
            case '>': return parse_greater();
            case '-': return parse_minus();
            case '"': return parse_string();
            case '@': return parse_at();
            case '0' ... '9': return parse_number();
            default: return parse_identifier_or_keyword();
        }
    }

  private:
    const std::string_view input_;
    std::size_t position_{};
    std::size_t line_{ 1 };
    std::size_t column_{ 1 };

    // NOTE: This function is not marked as nodiscard, since this is an
    // idiomatic pattern and we often need to call this function without
    // using its return value (function is allowed to have side effects)
    auto advance() noexcept -> char
    {
        if (at_end()) {
            return '\0';
        }

        const char c = input_[position_];
        ++position_;

        if (input_[position_] == '\n') {
            column_ = 1;
            ++line_;
        } else {
            ++column_;
        }

        return c;
    }

    [[nodiscard]]
    auto at_end() const noexcept -> bool
    {
        return position_ >= input_.size();
    }

    [[nodiscard]]
    auto peek() const noexcept -> char
    {
        return at_end() ? '\0' : input_[position_ + 1];
    }

    auto skip_whitespace() noexcept -> void
    {
        while (!at_end()) {
            if (std::isspace(peek())) {
                advance();
            } else {
                break;
            }
        }
    }

    //===---------------------------------------------------------------------===//
    // Parsing Helpers
    //===---------------------------------------------------------------------===//
    [[nodiscard]]
    auto parse_at() -> std::expected<Token, ParseError>
    {
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
    }

    [[nodiscard]]
    auto parse_bang() -> std::expected<Token, ParseError>
    {
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
    }

    [[nodiscard]]
    auto parse_dot() -> std::expected<Token, ParseError>
    {
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
    }

    [[nodiscard]]
    auto parse_equal() -> std::expected<Token, ParseError>
    {
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
    }

    [[nodiscard]]
    auto parse_greater() -> std::expected<Token, ParseError>
    {
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
    }

    [[nodiscard]]
    auto parse_less() -> std::expected<Token, ParseError>
    {
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
    }

    [[nodiscard]]
    auto parse_minus() -> std::expected<Token, ParseError>
    {
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
    }

    [[nodiscard]]
    auto parse_number() -> std::expected<Token, ParseError>
    {
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
    }

    [[nodiscard]]
    auto parse_single_char(TokenType token) -> std::expected<Token, ParseError>
    {
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
    }

    [[nodiscard]]
    auto parse_string() -> std::expected<Token, ParseError>
    {
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
    }

    [[nodiscard]]
    auto parse_identifier_or_keyword() -> std::expected<Token, ParseError>
    {
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;
    }
};

} // namespace kumi
