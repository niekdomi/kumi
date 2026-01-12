/// @file lexer.hpp
/// @brief Lexical analyzer
///
/// @see Token for token definitions
/// @see TokenType for all token types

#pragma once

#include "lang/lex/char_utils.hpp"
#include "lang/lex/token.hpp"
#include "lang/support/macros.hpp"
#include "lang/support/parse_error.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

namespace kumi::lang {

/// @brief Lexical analyzer that converts source text into tokens
class Lexer final
{
  public:
    /// @brief Constructs a lexer for the given input
    /// @param input Source text to tokenize
    explicit Lexer(std::string_view input) : input_(input) {}

    /// @brief Tokenizes the entire input into a vector of tokens
    /// @return Vector of tokens on success, ParseError on failure
    [[nodiscard]]
    auto tokenize() -> std::expected<std::vector<Token>, ParseError>
    {
        std::vector<Token> tokens{};
        tokens.reserve(input_.size());

        while (true) {
            const auto token = TRY(next_token());

            const bool is_eof = (token.type == TokenType::END_OF_FILE);
            tokens.push_back(token);

            if (is_eof) [[unlikely]] {
                break;
            }
        }

        return tokens;
    }

  private:
    std::string_view input_;    ///< Source text being tokenized
    std::uint32_t position_{0}; ///< Current position in input_

    /// @brief Advances to next character and updates position
    /// @return Current character before advancing, or '\0' if at EOF
    auto advance() noexcept -> char
    {
        return input_.at(position_++);
    }

    /// @brief Checks if lexer has reached end of input
    /// @return true if at EOF, false otherwise
    [[nodiscard]]
    auto at_end() const noexcept -> bool
    {
        return position_ >= input_.size();
    }

    /// @brief Matches and consumes a string if it appears at current position
    /// @param str String to match
    /// @return true if matched and consumed, false otherwise
    [[nodiscard]]
    auto match_string(std::string_view str) noexcept -> bool
    {
        if (input_.substr(position_).starts_with(str)) {
            position_ += str.size();
            return true;
        }
        return false;
    }

    /// @brief Reads and returns the next token from input
    /// @return Token on success, ParseError on failure
    [[nodiscard]]
    auto next_token() -> std::expected<Token, ParseError>
    {
        skip_whitespace();

        if (at_end()) [[unlikely]] {
            return Token{
              .value = "",
              .position = position_,
              .type = TokenType::END_OF_FILE,
            };
        }

        switch (peek()) {
            case '{':         return lex_single_char(TokenType::LEFT_BRACE, "{");
            case '}':         return lex_single_char(TokenType::RIGHT_BRACE, "}");
            case '[':         return lex_single_char(TokenType::LEFT_BRACKET, "[");
            case ']':         return lex_single_char(TokenType::RIGHT_BRACKET, "]");
            case '(':         return lex_single_char(TokenType::LEFT_PAREN, "(");
            case ')':         return lex_single_char(TokenType::RIGHT_PAREN, ")");
            case ':':         return lex_single_char(TokenType::COLON, ":");
            case ';':         return lex_single_char(TokenType::SEMICOLON, ";");
            case ',':         return lex_single_char(TokenType::COMMA, ",");
            case '?':         return lex_single_char(TokenType::QUESTION, "?");
            case '$':         return lex_single_char(TokenType::DOLLAR, "$");
            case '.':         return lex_dot();
            case '!':         return lex_bang();
            case '=':         return lex_equal();
            case '<':         return lex_less();
            case '>':         return lex_greater();
            case '"':         return lex_string();
            case '@':         return lex_at();
            case '0' ... '9': return lex_number();
            case '/':         return lex_comment();
            default:          return lex_identifier_or_keyword();
        }
    }

    /// @brief Peeks at next character without advancing
    /// @param k lookahead, defaults to 0
    /// @return Next character, or '\0' if at EOF
    [[nodiscard]]
    auto peek(std::uint32_t k = 0) const noexcept -> char
    {
        const auto pos = position_ + k;
        if (pos >= input_.size()) [[unlikely]] {
            return '\0';
        }
        return input_.at(pos);
    }

    /// @brief Skips whitespace characters (space, tab, newline, ...)
    auto skip_whitespace() noexcept -> void
    {
        while (is_space(peek())) {
            ++position_;
        }
    }

    //===------------------------------------------------------------------===//
    // Lexing Helpers
    //===------------------------------------------------------------------===//

    [[nodiscard]]
    auto lex_at() -> std::expected<Token, ParseError>
    {
        const auto start_pos = position_;

        static constexpr std::array<std::pair<std::string_view, TokenType>, 11> KEYWORDS = {
          // Top-Level Declaration
          std::pair{"@import",   TokenType::AT_IMPORT  },

          // Control Flow
          std::pair{"@if",       TokenType::AT_IF      },
          std::pair{"@else-if",  TokenType::AT_ELSE_IF },
          std::pair{"@else",     TokenType::AT_ELSE    },
          std::pair{"@for",      TokenType::AT_FOR     },
          std::pair{"@break",    TokenType::AT_BREAK   },
          std::pair{"@continue", TokenType::AT_CONTINUE},

          // Diagnostic Directives
          std::pair{"@error",    TokenType::AT_ERROR   },
          std::pair{"@warning",  TokenType::AT_WARNING },
          std::pair{"@info",     TokenType::AT_INFO    },
          std::pair{"@debug",    TokenType::AT_DEBUG   },
        };

        for (const auto& [keyword, type] : KEYWORDS) {
            if (match_string(keyword)) {
                return Token{
                  .value = keyword,
                  .position = start_pos,
                  .type = type,
                };
            }
        }

        return error<Token>("unexpected character after '@'", position_);
    }

    [[nodiscard]]
    auto lex_bang() -> std::expected<Token, ParseError>
    {
        const auto start_pos = position_;
        if (match_string("!=")) [[likely]] {
            return Token{
              .value = "!=",
              .position = start_pos,
              .type = TokenType::NOT_EQUAL,
            };
        }

        return error<Token>(
          std::format("unexpected character after '!': '{}'", peek()), position_, "expected '='");
    }

    [[nodiscard]]
    auto lex_comment() -> std::expected<Token, ParseError>
    {
        const auto start_pos = position_;

        // Line comment
        if (match_string("//")) {
            const auto remaining = input_.substr(position_);
            if (const auto pos = remaining.find('\n'); pos != std::string_view::npos) [[likely]]
            {
                position_ += pos;
            } else {
                // No newline found, consume until EOF
                position_ = static_cast<std::uint32_t>(input_.size());
            }
            return Token{
              .value = input_.substr(start_pos, position_ - start_pos),
              .position = start_pos,
              .type = TokenType::COMMENT,
            };
        }

        // Block comment
        if (match_string("/*")) {
            const auto remaining = input_.substr(position_);
            if (const auto pos = remaining.find("*/"); pos != std::string_view::npos) [[likely]]
            {
                position_ += pos + 2;
                return Token{
                  .value = input_.substr(start_pos, position_ - start_pos),
                  .position = start_pos,
                  .type = TokenType::COMMENT,
                };
            }
            // No closing found, set position to EOF
            position_ = static_cast<std::uint32_t>(input_.size());
            return error<Token>("unterminated block comment", position_, "missing closing */");
        }

        return error<Token>(std::format("unexpected character after '/': '{}'", peek()),
                            position_,
                            "expected '/' or '*'");
    }

    [[nodiscard]]
    auto lex_dot() -> std::expected<Token, ParseError>
    {
        const auto start_pos = position_;
        if (match_string("..")) [[likely]] {
            return Token{
              .value = "..",
              .position = start_pos,
              .type = TokenType::RANGE,
            };
        }

        return error<Token>(
          std::format("unexpected character after '.': '{}'", peek()), position_, "expected '.'");
    }

    [[nodiscard]]
    auto lex_equal() -> std::expected<Token, ParseError>
    {
        const auto start_pos = position_;
        if (match_string("==")) [[likely]] {
            return Token{
              .value = "==",
              .position = start_pos,
              .type = TokenType::EQUAL,
            };
        }

        return error<Token>(
          std::format("unexpected character after '=': '{}'", peek()), position_, "expected '='");
    }

    [[nodiscard]]
    auto lex_greater() noexcept -> Token
    {
        const auto start_pos = position_;

        if (match_string(">=")) {
            return Token{
              .value = ">=",
              .position = start_pos,
              .type = TokenType::GREATER_EQUAL,
            };
        }

        advance();
        return Token{
          .value = ">",
          .position = start_pos,
          .type = TokenType::GREATER,
        };
    }

    [[nodiscard]]
    auto lex_less() noexcept -> Token
    {
        const auto start_pos = position_;

        if (match_string("<=")) {
            return Token{
              .value = "<=",
              .position = start_pos,
              .type = TokenType::LESS_EQUAL,
            };
        }

        advance();
        return Token{
          .value = "<",
          .position = start_pos,
          .type = TokenType::LESS,
        };
    }

    [[nodiscard]]
    auto lex_number() noexcept -> Token
    {
        const auto start_pos = position_;

        while (is_digit(peek())) {
            ++position_;
        }

        const auto num_str = input_.substr(start_pos, position_ - start_pos);
        return Token{
          .value = num_str,
          .position = start_pos,
          .type = TokenType::NUMBER,
        };
    }

    [[nodiscard]]
    auto lex_single_char(TokenType token, std::string_view value) noexcept -> Token
    {
        const auto start_pos = position_;
        advance();
        return Token{
          .value = value,
          .position = start_pos,
          .type = token,
        };
    }

    [[nodiscard]]
    auto lex_string() -> std::expected<Token, ParseError>
    {
        const auto start_pos = position_;

        advance(); // Consume opening "

        while (peek() != '"') {
            if (at_end()) [[unlikely]] {
                return error<Token>("unterminated string literal", start_pos, "missing closing \"");
            }

            const char c = peek();

            if (c == '\n' || c == '\r') [[unlikely]] {
                return error<Token>("unterminated string literal",
                                    start_pos,
                                    "missing closing \"",
                                    "strings cannot span multiple lines");
            }

            if (c == '\\') {
                advance(); // Consume '\'
                // Validate escape sequence
                if (const char next = peek();
                    next != '"' && next != 'n' && next != 't' && next != 'r' && next != '\\')
                  [[unlikely]]
                {
                    return error<Token>(std::format("invalid escape sequence: '\\{}'", next),
                                        position_,
                                        "unknown escape character",
                                        R"(valid escapes: \", \n, \t, \r, \\)");
                }
                advance(); // Consume escaped character
            } else {
                advance(); // Consume regular character
            }
        }

        advance(); // Consume closing "

        // Return raw string including quotes from original input
        const auto str = input_.substr(start_pos, position_ - start_pos);
        return Token{
          .value = str,
          .position = start_pos,
          .type = TokenType::STRING,
        };
    }

    [[nodiscard]]
    auto lex_identifier_or_keyword() -> std::expected<Token, ParseError>
    {
        const auto start_pos = position_;

        while (is_identifier(peek())) {
            ++position_;
        }

        const auto text = input_.substr(start_pos, position_ - start_pos);

        // No valid identifier characters were consumed
        if (text.empty()) {
            return error<Token>(std::format("unexpected character: '{}'", peek()),
                                position_,
                                "invalid character here",
                                "expected an identifier, keyword, or other valid token");
        }

        static constexpr std::array<std::pair<std::string_view, TokenType>, 20> KEYWORDS = {
          // Top-Level Declarations
          std::pair{"project",      TokenType::PROJECT     },
          std::pair{"workspace",    TokenType::WORKSPACE   },
          std::pair{"target",       TokenType::TARGET      },
          std::pair{"dependencies", TokenType::DEPENDENCIES},
          std::pair{"options",      TokenType::OPTIONS     },
          std::pair{"mixin",        TokenType::MIXIN       },
          std::pair{"profile",      TokenType::PROFILE     },
          std::pair{"install",      TokenType::INSTALL     },
          std::pair{"package",      TokenType::PACKAGE     },
          std::pair{"scripts",      TokenType::SCRIPTS     },
          std::pair{"with",         TokenType::WITH        },

          // Visibility Modifiers
          std::pair{"public",       TokenType::PUBLIC      },
          std::pair{"private",      TokenType::PRIVATE     },
          std::pair{"interface",    TokenType::INTERFACE   },

          // Control Flow
          std::pair{"in",           TokenType::IN          },

          // Logical Operators
          std::pair{"and",          TokenType::AND         },
          std::pair{"or",           TokenType::OR          },
          std::pair{"not",          TokenType::NOT         },

          // Literals
          std::pair{"true",         TokenType::TRUE        },
          std::pair{"false",        TokenType::FALSE       },
        };

        for (const auto& [keyword, type] : KEYWORDS) {
            if (text == keyword) {
                return Token{
                  .value = keyword,
                  .position = start_pos,
                  .type = type,
                };
            }
        }

        // If it's not a keyword, it's an identifier
        return Token{
          .value = text,
          .position = start_pos,
          .type = TokenType::IDENTIFIER,
        };
    }
};

} // namespace kumi::lang

