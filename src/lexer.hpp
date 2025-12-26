/// @file lexer.hpp
/// @brief Lexical analyzer
///
/// @see Token for token definitions
/// @see TokenType for all token types

#pragma once

#include "error.hpp"
#include "macros.hpp"
#include "token.hpp"

#include <cctype>
#include <cstddef>
#include <expected>
#include <format>
#include <string_view>
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
        tokens.reserve(256);

        while (true) {
            auto token = TRY(next_token());
            tokens.push_back(std::move(token));

            if (token.type == TokenType::END_OF_FILE) {
                break;
            }
        }

        return tokens;
    }

  private:
    std::string_view input_; ///< Source text being tokenized

    std::size_t line_{ 1 };     ///< Current line number
    std::size_t column_{ 1 };   ///< Current column number
    std::size_t position_{ 0 }; ///< Current position in input_

    /// @brief Advances to next character and updates position/line/column
    /// @return Current character before advancing, or '\0' if at EOF
    /// @note Handles Unix (\n), Windows (\r\n), and old Mac (\r) line endings
    auto advance() noexcept -> char
    {
        if (at_end()) {
            return '\0';
        }

        const char c = input_[position_];
        ++position_;

        if (c == '\n') {
            ++line_;
            column_ = 1;
        } else if (c == '\r') {
            ++line_;
            column_ = 1;
            if (peek() == '\n') {
                ++position_;
            }
        } else {
            ++column_;
        }

        return c;
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
    auto match_string(std::string_view str) noexcept -> bool
    {
        // TODO(domi): Assumes no newlines in str; column tracking need verification
        // + tests
        if (input_.substr(position_).starts_with(str)) {
            position_ += str.size();
            column_ += str.size();
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

        if (at_end()) {
            return Token{
                .type = TokenType::END_OF_FILE,
                .value = "",
                .line = line_,
                .column = column_,
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
        return pos >= input_.size() ? '\0' : input_[pos];
    }

    /// @brief Skips whitespace characters (space, tab, newline, ...)
    auto skip_whitespace_and_comment() noexcept -> void
    {
        while (!at_end()) {
            if (std::isspace(peek())) {
                advance();
                continue;
            }

            // Line comment: '//'
            if (peek() == '/' && peek(1) == '/') {
                advance();
                advance();

                while (!at_end() && peek() != '\n') {
                    advance();
                }
                continue;
            }

            // Block comment: '/* ... */'
            if (peek() == '/' && peek(1) == '*') {
                advance();
                advance();

                while (!at_end()) {
                    if (peek() == '*' && peek(1) == '/') {
                        advance();
                        advance();
                        break;
                    }
                    advance();
                }
                continue;
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
        const auto start_line = line_;
        const auto start_column = column_;

        static constexpr std::array KEYWORDS = {
            std::pair{ "if",       TokenType::IF             },
            std::pair{ "else-if",  TokenType::ELSE_IF        },
            std::pair{ "else",     TokenType::ELSE           },
            std::pair{ "for",      TokenType::FOR            },
            std::pair{ "in",       TokenType::IN             },
            std::pair{ "break",    TokenType::BREAK          },
            std::pair{ "continue", TokenType::CONTINUE       },
            std::pair{ "error",    TokenType::ERROR          },
            std::pair{ "warning",  TokenType::WARNING        },
            std::pair{ "info",     TokenType::INFO           },
            std::pair{ "import",   TokenType::IMPORT_KEYWORD },
            std::pair{ "apply",    TokenType::APPLY_KEYWORD  },
        };

        for (const auto [keyword, type] : KEYWORDS) {
            if (match_string(keyword)) {
                return Token{
                    .type = type,
                    .value = std::format("@{}", keyword),
                    .line = start_line,
                    .column = start_column,
                };
            }
        }

        return error<Token>(
          std::format("unexpected character after '@': '{}'", peek()), line_, column_);
    }

    [[nodiscard]]
    auto lex_bang() noexcept -> Token
    {
        const auto start_line = line_;
        const auto start_column = column_;

        if (match_string("!=")) {
            return Token{
                .type = TokenType::NOT_EQUAL,
                .value = "!=",
                .line = start_line,
                .column = start_column,
            };
        }

        return Token{
            .type = TokenType::EXCLAMATION,
            .value = "!",
            .line = start_line,
            .column = start_column,
        };
    }

    [[nodiscard]]
    auto lex_dot() noexcept -> Token
    {
        const auto start_line = line_;
        const auto start_column = column_;

        if (match_string("...")) {
            return Token{
                .type = TokenType::ELLIPSIS,
                .value = "...",
                .line = start_line,
                .column = start_column,
            };
        }

        if (match_string("..")) {
            return Token{
                .type = TokenType::RANGE,
                .value = "..",
                .line = start_line,
                .column = start_column,
            };
        }

        return Token{
            .type = TokenType::DOT,
            .value = ".",
            .line = start_line,
            .column = start_column,
        };
    }

    [[nodiscard]]
    auto lex_equal() noexcept -> Token
    {
        const auto start_line = line_;
        const auto start_column = column_;

        if (match_string("==")) {
            return Token{
                .type = TokenType::EQUAL,
                .value = "==",
                .line = start_line,
                .column = start_column,
            };
        }

        return Token{
            .type = TokenType::ASSIGN,
            .value = "=",
            .line = start_line,
            .column = start_column,
        };
    }

    [[nodiscard]]
    auto lex_greater() noexcept -> Token
    {
        const auto start_line = line_;
        const auto start_column = column_;

        if (match_string(">=")) {
            return Token{
                .type = TokenType::GREATER_EQUAL,
                .value = ">=",
                .line = start_line,
                .column = start_column,
            };
        }

        return Token{
            .type = TokenType::GREATER,
            .value = ">",
            .line = start_line,
            .column = start_column,
        };
    }

    [[nodiscard]]
    auto lex_less() noexcept -> Token
    {
        const auto start_line = line_;
        const auto start_column = column_;

        if (match_string("<=")) {
            return Token{
                .type = TokenType::LESS_EQUAL,
                .value = "<=",
                .line = start_line,
                .column = start_column,
            };
        }

        return Token{
            .type = TokenType::LESS,
            .value = "<",
            .line = start_line,
            .column = start_column,
        };
    }

    [[nodiscard]]
    auto lex_minus() -> std::expected<Token, ParseError>
    {
        const auto start_line = line_;
        const auto start_column = column_;

        if (match_string("->")) {
            return Token{
                .type = TokenType::ARROW,
                .value = "->",
                .line = start_line,
                .column = start_column,
            };
        }

        if (match_string("--")) {
            return Token{
                .type = TokenType::MINUS_MINUS,
                .value = "--",
                .line = start_line,
                .column = start_column,
            };
        }

        return error<Token>(
          std::format("unexpected character after '-': '{}'", peek()), line_, column_);
    }

    [[nodiscard]]
    auto lex_number() noexcept -> Token
    {
        const auto start_line = line_;
        const auto start_column = column_;
        const auto start_position = position_;

        while (std::isdigit(peek())) {
            advance();
        }

        const auto num_str = input_.substr(start_position, position_ - start_position);
        return Token{
            .type = TokenType::NUMBER,
            .value = std::string(num_str),
            .line = start_line,
            .column = start_column,
        };
    }

    [[nodiscard]]
    auto lex_single_char(TokenType token, std::string_view value) noexcept -> Token
    {
        const auto start_line = line_;
        const auto start_column = column_;

        advance();

        return Token{
            .type = token,
            .value = std::string(value),
            .line = start_line,
            .column = start_column,
        };
    }

    [[nodiscard]]
    auto lex_string() -> std::expected<Token, ParseError>
    {
        const auto start_line = line_;
        const auto start_column = column_;

        advance(); // Consume opening "

        std::string str{};
        str.reserve(32);
        while (peek() != '"') {
            if (at_end()) {
                return error<Token>("unterminated string literal (EOF)", line_, column_);
            }

            const char c = peek();
            if (c == '\n' || c == '\r') {
                return error<Token>("unterminated string literal (EOL)", line_, column_);
            }

            if (peek() == '\\') {
                advance(); // Consume '\'

                switch (peek()) {
                    case '"':  str += '"'; break;
                    case 'n':  str += '\n'; break;
                    case 't':  str += '\t'; break;
                    case 'r':  str += '\r'; break;
                    case '\\': str += '\\'; break;
                    default:
                        return error<Token>(
                          std::format("invalid escape sequence: '\\{}'", peek()), line_, column_);
                }
                advance();
            } else {
                str += advance();
            }
        }

        advance(); // Consume closing "

        return Token{
            .type = TokenType::STRING,
            .value = str,
            .line = start_line,
            .column = start_column,
        };
    }

    [[nodiscard]]
    auto lex_identifier_or_keyword() noexcept -> Token
    {
        const auto start_line = line_;
        const auto start_column = column_;
        const auto start_position = position_;

        while (std::isalnum(peek()) || peek() == '_' || peek() == '-') {
            advance();
        }

        const auto text = input_.substr(start_position, position_ - start_position);

        static constexpr std::array KEYWORDS = {
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

            // Keywords - Visibility
            std::pair{ "public",       TokenType::PUBLIC       },
            std::pair{ "private",      TokenType::PRIVATE      },
            std::pair{ "interface",    TokenType::INTERFACE    },

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

        for (const auto [keyword, type] : KEYWORDS) {
            if (text == keyword) {
                return Token{
                    .type = type,
                    .value = std::string(text),
                    .line = start_line,
                    .column = start_column,
                };
            }
        }

        // If it's not a keyword, it's an identifier
        return Token{
            .type = TokenType::IDENTIFIER,
            .value = std::string(text),
            .line = start_line,
            .column = start_column,
        };
    }
};

} // namespace kumi
