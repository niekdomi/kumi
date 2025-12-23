#pragma once

#include "error.hpp"
#include "token.hpp"

#include <cctype>
#include <cstddef>
#include <expected>
#include <format>
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
            case '{': return lex_single_char(TokenType::LEFT_BRACE, "{");
            case '}': return lex_single_char(TokenType::RIGHT_BRACE, "}");
            case '[': return lex_single_char(TokenType::LEFT_BRACKET, "[");
            case ']': return lex_single_char(TokenType::RIGHT_BRACKET, "]");
            case '(': return lex_single_char(TokenType::LEFT_PAREN, "(");
            case ')': return lex_single_char(TokenType::RIGHT_PAREN, ")");
            case ':': return lex_single_char(TokenType::COLON, ":");
            case ';': return lex_single_char(TokenType::SEMICOLON, ";");
            case ',': return lex_single_char(TokenType::COMMA, ",");
            case '?': return lex_single_char(TokenType::QUESTION, "?");
            case '$': return lex_single_char(TokenType::DOLLAR, "$");
            case '.': return lex_dot();
            case '!': return lex_bang();
            case '=': return lex_equal();
            case '<': return lex_less();
            case '>': return lex_greater();
            case '-': return lex_minus();
            case '"': return lex_string();
            case '@': return lex_at();
            case '0' ... '9': return lex_number();
            default: return lex_identifier_or_keyword();
        }
    }

  private:
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const std::string_view input_;

    std::size_t line_{ 1 };
    std::size_t column_{ 1 };
    std::size_t position_{ 0 };

    // NOTE: This function is not marked as nodiscard, since this is an
    // idiomatic pattern and we often need to call this function without
    // using its return value (function is allowed to have side effects)
    //
    // Unix:    \n
    // Windows: \r\n
    // Old Mac: \r
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

    [[nodiscard]]
    auto at_end() const noexcept -> bool
    {
        return position_ >= input_.size();
    }

    template<typename T>
    [[nodiscard]]
    auto error(std::string_view message) const -> std::expected<T, ParseError>
    {
        return std::unexpected(ParseError{
          .message = std::string(message),
          .line = line_,
          .column = column_,
        });
    }

    // TODO(domi): This is quite optimized but assumes that there is no new
    // line so we need to create some test cases to verify that column_ is
    // correctly updated
    auto match_string(std::string_view str) noexcept -> bool
    {
        if (input_.substr(position_).starts_with(str)) {
            position_ += str.size();
            column_ += str.size();
            return true;
        }
        return false;
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

        return error<Token>(std::format("unexpected character after '@': '{}'", peek()));
    }

    [[nodiscard]]
    auto lex_bang() -> std::expected<Token, ParseError>
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
    auto lex_dot() -> std::expected<Token, ParseError>
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
    auto lex_equal() -> std::expected<Token, ParseError>
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
    auto lex_greater() -> std::expected<Token, ParseError>
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
    auto lex_less() -> std::expected<Token, ParseError>
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

        return error<Token>(std::format("unexpected character after '-': '{}'", peek()));
    }

    // NOTE: Kumi only supports integer numbers
    // This makes it simpler and there is also no real benefit to support:
    // - floating point numbers
    // - scientific notation
    // - hex, octal, binary numbers
    // - negative numbers (or prefixed with + or -)
    //
    // This only adds complexity in configuration and parsing without any real
    // benefit.
    [[nodiscard]]
    auto lex_number() -> std::expected<Token, ParseError>
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
    auto lex_single_char(TokenType token, std::string_view value)
      -> std::expected<Token, ParseError>
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
                return error<Token>("unterminated string literal (EOF)");
            }

            const char c = peek();
            if (c == '\n' || c == '\r') {
                return error<Token>("unterminated string literal (EOL)");
            }

            if (peek() == '\\') {
                advance(); // Consume '\'

                switch (peek()) {
                    case '"': str += '"'; break;
                    case '\\': str += '\\'; break;
                    case 'n': str += '\n'; break;
                    case 't': str += '\t'; break;
                    case 'r': str += '\r'; break;
                    default:
                        return error<Token>(std::format("invalid escape sequence: '\\{}'", peek()));
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
    auto lex_identifier_or_keyword() -> std::expected<Token, ParseError>
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
            std::pair{ "deps",         TokenType::DEPS         },
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
