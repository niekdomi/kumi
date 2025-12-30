/// @file lexer.hpp
/// @brief Lexical analyzer
///
/// @see Token for token definitions
/// @see TokenType for all token types

#pragma once

#include "lex/char_utils.hpp"
#include "lex/token.hpp"
#include "support/macros.hpp"
#include "support/parse_error.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kumi {

/// @brief Lexical analyzer that converts source text into tokens
class Lexer final
{
  public:
    /// @brief Constructs a lexer for the given input
    /// @param input Source text to tokenize
    explicit Lexer(std::string_view input) : input_(input) {};

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
    std::size_t position_{ 0 }; ///< Current position in input_

    /// @brief Advances to next character and updates position
    /// @return Current character before advancing, or '\0' if at EOF
    auto advance() noexcept -> char { return input_[position_++]; }

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
            const auto start_pos = position_;
            return Token{
                .value = "",
                .position = start_pos,
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
    /// @return Next character, or '\0' if at EOF
    [[nodiscard]]
    auto peek(std::size_t look_ahead = 0) const noexcept -> char
    {
        const auto pos = position_ + look_ahead;
        if (pos >= input_.size()) [[unlikely]] {
            return '\0';
        }
        return input_[pos];
    }

    /// @brief Skips whitespace characters (space, tab, newline, ...)
    auto skip_whitespace() noexcept -> void
    {
        while (is_space(peek())) {
            ++position_;
        }
    }

    //===---------------------------------------------------------------------===//
    // Lexing Helpers
    //===---------------------------------------------------------------------===//

    [[nodiscard]]
    auto lex_at() -> std::expected<Token, ParseError>
    {
        const auto start_pos = position_;

        static constexpr std::array<std::pair<std::string_view, TokenType>, 11> KEYWORDS = {
            // Top-Level Declaration
            std::pair{ "@import",   TokenType::AT_IMPORT   },

            // Control Flow
            std::pair{ "@if",       TokenType::AT_IF       },
            std::pair{ "@else-if",  TokenType::AT_ELSE_IF  },
            std::pair{ "@else",     TokenType::AT_ELSE     },
            std::pair{ "@for",      TokenType::AT_FOR      },
            std::pair{ "@break",    TokenType::AT_BREAK    },
            std::pair{ "@continue", TokenType::AT_CONTINUE },

            // Diagnostic Directives
            std::pair{ "@error",    TokenType::AT_ERROR    },
            std::pair{ "@warning",  TokenType::AT_WARNING  },
            std::pair{ "@info",     TokenType::AT_INFO     },
            std::pair{ "@debug",    TokenType::AT_DEBUG    },
        };

        for (const auto &[keyword, type] : KEYWORDS) {
            if (match_string(keyword)) {
                return Token{
                    .value = keyword,
                    .position = start_pos,
                    .type = type,
                };
            }
        }

        return error<Token>(std::format("unexpected character after '@': '{}'", peek()),
                            position_,
                            "expected one of: if, else-if, else, for, break, info, debug, import, "
                            "continue, error, warning");
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
            while (peek() != '\n' && !at_end()) {
                ++position_;
            }

            return Token{
                .value = input_.substr(start_pos, position_ - start_pos),
                .position = start_pos,
                .type = TokenType::COMMENT,
            };
        }

        // Block comment
        if (match_string("/*")) {
            while (!at_end()) {
                if (match_string("*/")) {
                    break;
                }
                ++position_;
            }

            return Token{
                .value = input_.substr(start_pos, position_ - start_pos),
                .position = start_pos,
                .type = TokenType::COMMENT,
            };
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

        std::string str;
        str.reserve(32);

        const auto is_string_char = [this]() -> bool {
            const char c = peek();
            return c != '"' && c != '\\' && c != '\n' && c != '\r';
        };

        while (peek() != '"') {
            if (at_end()) [[unlikely]] {
                return error<Token>("unterminated string literal", position_, "missing closing \"");
            }

            const auto batch_start = position_;
            while (is_string_char()) {
                position_++;
            }
            if (position_ > batch_start) {
                str.append(input_.substr(batch_start, position_ - batch_start));
            }

            const char c = peek();
            if (c == '\n' || c == '\r') [[unlikely]] {
                return error<Token>(
                  "unterminated string literal", position_, "strings cannot span multiple lines");
            }

            if (c == '\\') [[unlikely]] {
                advance(); // Consume '\'
                switch (peek()) {
                    case '"':  str += '"'; break;
                    case 'n':  str += '\n'; break;
                    case 't':  str += '\t'; break;
                    case 'r':  str += '\r'; break;
                    case '\\': str += '\\'; break;
                    default:
                        return error<Token>(std::format("invalid escape sequence: '\\{}'", peek()),
                                            position_,
                                            R"(valid escapes: \", \n, \t, \r, \\)");
                }
                advance();
            }
        }

        advance(); // Consume closing "

        return Token{
            .value = str,
            .position = start_pos,
            .type = TokenType::STRING,
        };
    }

    [[nodiscard]]
    auto lex_identifier_or_keyword() noexcept -> Token
    {
        const auto start_pos = position_;

        while (is_identifier(peek())) {
            ++position_;
        }

        const auto text = input_.substr(start_pos, position_ - start_pos);

        static constexpr std::array<std::pair<std::string_view, TokenType>, 19> KEYWORDS = {
            // Top-Level Declarations
            std::pair{ "project",      TokenType::PROJECT      },
            std::pair{ "workspace",    TokenType::WORKSPACE    },
            std::pair{ "target",       TokenType::TARGET       },
            std::pair{ "dependencies", TokenType::DEPENDENCIES },
            std::pair{ "options",      TokenType::OPTIONS      },
            std::pair{ "mixin",        TokenType::MIXIN        },
            std::pair{ "profile",      TokenType::PROFILE      },
            std::pair{ "install",      TokenType::INSTALL      },
            std::pair{ "package",      TokenType::PACKAGE      },
            std::pair{ "scripts",      TokenType::SCRIPTS      },
            std::pair{ "with",         TokenType::WITH         },

            // Visibility Modifiers
            std::pair{ "public",       TokenType::PUBLIC       },
            std::pair{ "private",      TokenType::PRIVATE      },
            std::pair{ "interface",    TokenType::INTERFACE    },

            // Logical Operators
            std::pair{ "and",          TokenType::AND          },
            std::pair{ "or",           TokenType::OR           },
            std::pair{ "not",          TokenType::NOT          },

            // Literals
            std::pair{ "true",         TokenType::TRUE         },
            std::pair{ "false",        TokenType::FALSE        },
        };

        for (const auto &[keyword, type] : KEYWORDS) {
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

} // namespace kumi
