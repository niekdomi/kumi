/// @file lexer.hpp
/// @brief Lexical analyzer
///
/// @see Token for token definitions
/// @see TokenType for all token types

#pragma once

#include "lex/char_class.hpp"
#include "lex/token.hpp"
#include "support/error.hpp"
#include "support/macros.hpp"

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
        // Heuristic: average token is ~4 characters, reserve accordingly
        // Add 16 to handle edge cases and avoid reallocation for small inputs
        tokens.reserve((input_.size() / 4) + 16);

        while (true) {
            const auto token = TRY(next_token());

            const bool is_eof = token.type == TokenType::END_OF_FILE;
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
    auto advance() noexcept -> char
    {
        if (at_end()) [[unlikely]] {
            return '\0';
        }

        return input_[position_++];
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
        skip_whitespace_and_comment();

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
            case '-':         return lex_minus();
            case '"':         return lex_string();
            case '@':         return lex_at();
            case '0' ... '9': return lex_number();
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
    auto skip_whitespace_and_comment() noexcept -> void
    {
        while (!at_end()) [[likely]] {
            const char c = peek();

            if (is_space(c)) {
                position_++;
                continue;
            }

            if (c == '/') {
                const char next = peek(1);

                // Line comment: '//'
                if (next == '/') {
                    position_ += 2;
                    while (!at_end() && peek() != '\n') {
                        position_++;
                    }
                    continue;
                }

                // Block comment: '/* ... */'
                if (next == '*') {
                    position_ += 2;
                    while (!at_end()) {
                        if (peek() == '*' && peek(1) == '/') {
                            position_ += 2;
                            break;
                        }
                        position_++;
                    }
                    continue;
                }
            }

            break;
        }
    }

    //===---------------------------------------------------------------------===//
    // Lexing Helpers
    //===---------------------------------------------------------------------===//

    [[nodiscard]]
    auto lex_at() -> std::expected<Token, ParseError>
    {
        const auto start_pos = position_;

        static constexpr std::array KEYWORDS = {
            std::pair{ "@if",       TokenType::IF             },
            std::pair{ "@else-if",  TokenType::ELSE_IF        },
            std::pair{ "@else",     TokenType::ELSE           },
            std::pair{ "@for",      TokenType::FOR            },
            std::pair{ "@break",    TokenType::BREAK          },
            std::pair{ "@continue", TokenType::CONTINUE       },
            std::pair{ "@error",    TokenType::ERROR          },
            std::pair{ "@warning",  TokenType::WARNING        },
            std::pair{ "@info",     TokenType::INFO           },
            std::pair{ "@import",   TokenType::IMPORT_KEYWORD },
            std::pair{ "@apply",    TokenType::APPLY_KEYWORD  },
        };

        for (const auto &[keyword, type] : KEYWORDS) {
            if (match_string(keyword)) {
                return Token{
                    .value = std::string(keyword),
                    .position = start_pos,
                    .type = type,
                };
            }
        }

        return error<Token>(std::format("unexpected character after '@': '{}'", peek()), position_);
    }

    [[nodiscard]]
    auto lex_bang() noexcept -> Token
    {
        const auto start_pos = position_;

        if (match_string("!=")) {
            return Token{
                .value = "!=",
                .position = start_pos,
                .type = TokenType::NOT_EQUAL,
            };
        }

        advance();
        return Token{
            .value = "!",
            .position = start_pos,
            .type = TokenType::EXCLAMATION,
        };
    }

    [[nodiscard]]
    auto lex_dot() noexcept -> Token
    {
        const auto start_pos = position_;

        if (match_string("...")) {
            return Token{
                .value = "...",
                .position = start_pos,
                .type = TokenType::ELLIPSIS,
            };
        }

        if (match_string("..")) {
            return Token{
                .value = "..",
                .position = start_pos,
                .type = TokenType::RANGE,
            };
        }

        advance();
        return Token{
            .value = ".",
            .position = start_pos,
            .type = TokenType::DOT,
        };
    }

    [[nodiscard]]
    auto lex_equal() noexcept -> Token
    {
        const auto start_pos = position_;

        if (match_string("==")) {
            return Token{
                .value = "==",
                .position = start_pos,
                .type = TokenType::EQUAL,
            };
        }

        advance();
        return Token{
            .value = "=",
            .position = start_pos,
            .type = TokenType::ASSIGN,
        };
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
    auto lex_minus() -> std::expected<Token, ParseError>
    {
        const auto start_pos = position_;

        if (match_string("->")) {
            return Token{
                .value = "->",
                .position = start_pos,
                .type = TokenType::ARROW,
            };
        }

        if (match_string("--")) {
            return Token{
                .value = "--",
                .position = start_pos,
                .type = TokenType::MINUS_MINUS,
            };
        }

        return error<Token>(std::format("unexpected character after '-': '{}'", peek()), position_);
    }

    [[nodiscard]]
    auto lex_number() noexcept -> Token
    {
        const auto start_pos = position_;

        while (is_digit(peek())) {
            advance();
        }

        const auto num_str = input_.substr(start_pos, position_ - start_pos);
        return Token{
            .value = std::string(num_str),
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
            .value = std::string(value),
            .position = start_pos,
            .type = token,
        };
    }

    [[nodiscard]]
    auto lex_string() -> std::expected<Token, ParseError>
    {
        const auto start_pos = position_;

        advance(); // Consume opening "

        std::string str{};
        str.reserve(32);
        while (peek() != '"') {
            if (at_end()) [[unlikely]] {
                return error<Token>("unterminated string literal (EOF)", position_);
            }

            const char c = peek();
            if (c == '\n' || c == '\r') [[unlikely]] {
                return error<Token>("unterminated string literal (EOL)", position_);
            }

            if (peek() == '\\') {
                advance(); // Consume '\'

                switch (peek()) {
                    case '"':  str += '"'; break;
                    case 'n':  str += '\n'; break;
                    case 't':  str += '\t'; break;
                    case 'r':  str += '\r'; break;
                    case '\\': str += '\\'; break;
                    default:   {
                        return error<Token>(std::format("invalid escape sequence: '\\{}'", peek()),
                                            position_);
                    }
                }
                advance();
            } else {
                str += advance();
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

        if (!is_alpha(peek()) && peek() != '_') [[unlikely]] {
            // Invalid identifier, will be handled as an error in the parser
            return Token{
                .value = std::string(1, advance()),
                .position = start_pos,
                .type = TokenType::IDENTIFIER,
            };
        }

        advance();

        while (is_ident(peek())) {
            advance();
        }

        const auto text = input_.substr(start_pos, position_ - start_pos);

        static constexpr std::array<std::pair<std::string_view, TokenType>, 46> KEYWORDS = {
            // Keywords - Top level
            std::pair{ "project",      TokenType::PROJECT      },
            std::pair{ "workspace",    TokenType::WORKSPACE    },
            std::pair{ "target",       TokenType::TARGET       },
            std::pair{ "dependencies", TokenType::DEPENDENCIES },
            std::pair{ "options",      TokenType::OPTIONS      },
            std::pair{ "global",       TokenType::GLOBAL       },
            std::pair{ "mixin",        TokenType::MIXIN        },
            std::pair{ "preset",       TokenType::PRESET       },
            std::pair{ "features",     TokenType::FEATURES     },
            std::pair{ "testing",      TokenType::TESTING      },
            std::pair{ "install",      TokenType::INSTALL      },
            std::pair{ "package",      TokenType::PACKAGE      },
            std::pair{ "scripts",      TokenType::SCRIPTS      },
            std::pair{ "toolchain",    TokenType::TOOLCHAIN    },
            std::pair{ "root",         TokenType::ROOT         },
            std::pair{ "in",           TokenType::IN           },

            // Keywords - Visibility
            std::pair{ "public",       TokenType::PUBLIC       },
            std::pair{ "private",      TokenType::PRIVATE      },
            std::pair{ "interface",    TokenType::INTERFACE    },

            // Keywords - Values
            std::pair{ "true",         TokenType::TRUE         },
            std::pair{ "false",        TokenType::FALSE        },

            // Keywords - Properties
            std::pair{ "type",         TokenType::TYPE         },
            std::pair{ "sources",      TokenType::SOURCES      },
            std::pair{ "headers",      TokenType::HEADERS      },
            std::pair{ "depends",      TokenType::DEPENDS      },
            std::pair{ "apply",        TokenType::APPLY        },
            std::pair{ "inherits",     TokenType::INHERITS     },
            std::pair{ "extends",      TokenType::EXTENDS      },
            std::pair{ "export",       TokenType::EXPORT       },
            std::pair{ "import",       TokenType::IMPORT       },

            // Keywords - Logical
            std::pair{ "and",          TokenType::AND          },
            std::pair{ "or",           TokenType::OR           },
            std::pair{ "not",          TokenType::NOT          },

            // Keywords - Functions
            std::pair{ "platform",     TokenType::PLATFORM     },
            std::pair{ "arch",         TokenType::ARCH         },
            std::pair{ "compiler",     TokenType::COMPILER     },
            std::pair{ "config",       TokenType::CONFIG       },
            std::pair{ "option",       TokenType::OPTION       },
            std::pair{ "feature",      TokenType::FEATURE      },
            std::pair{ "has-feature",  TokenType::HAS_FEATURE  },
            std::pair{ "exists",       TokenType::EXISTS       },
            std::pair{ "var",          TokenType::VAR          },
            std::pair{ "glob",         TokenType::GLOB         },
            std::pair{ "git",          TokenType::GIT          },
            std::pair{ "url",          TokenType::URL          },
            std::pair{ "system",       TokenType::SYSTEM       },
        };

        for (const auto &[keyword, type] : KEYWORDS) {
            if (text == keyword) {
                return Token{
                    .value = std::string(text),
                    .position = start_pos,
                    .type = type,
                };
            }
        }

        // If it's not a keyword, it's an identifier
        return Token{
            .value = std::string(text),
            .position = start_pos,
            .type = TokenType::IDENTIFIER,
        };
    }
};

} // namespace kumi
