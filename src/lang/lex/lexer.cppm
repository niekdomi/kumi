module;
#include "lang/support/macros.hpp"

export module kumi.lang.lex;
import std;

import kumi.lang.lex.chars;
import kumi.lang.lex.token;
import kumi.lang.support;

/// @file lexer.hpp
/// @brief Lexical analyzer
///
/// @see Token for token definitions
/// @see TokenType for all token types

export namespace kumi::lang {

/// @brief Lexical analyzer that converts source text into a token stream
///
/// Single-pass, position-based tokenizer. Comments are not emitted as tokens;
/// instead they are attached to the neighboring real token via Token::leading
/// (comment appears before the token, possibly on a prior line) or
/// Token::trailing (comment appears after the token on the same line).
/// Both fields store (comment_start_pos + 1) so that 0 is a "none" sentinel.
///
/// tokenize() returns all tokens including the final END_OF_FILE, paired with
/// any errors encountered. Lexing continues after an error to collect as many
/// diagnostics as possible.
class Lexer final
{
  public:
    explicit Lexer(std::string_view input) : input_(input) {}

    [[nodiscard]]
    auto tokenize() && -> std::pair<std::vector<Token>, std::vector<Diagnostics>>
    {
        tokens_.reserve(input_.size());
        std::vector<Diagnostics> errors;

        while (true) {
            const auto pre_pos = position_;

            auto result = next_token();

            if (result) {
                const bool eof = result->kind == TokenType::END_OF_FILE;
                tokens_.push_back(*result);
                if (eof) {
                    break;
                }
            } else {
                errors.push_back(std::move(result.error()));
                if (position_ == pre_pos && !at_end()) {
                    advance();
                }
            }
        }

        return {std::move(tokens_), std::move(errors)};
    }

  private:
    std::string_view input_;    ///< Input source
    std::uint32_t position_{0}; ///< Current position in the input
    std::vector<Token> tokens_; ///< Collected tokens
    /// Position of the first leading comment
    std::optional<std::uint32_t> first_leading_comment_pos_;

    __attribute__((always_inline)) auto advance() noexcept -> void
    {
        ++position_;
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto at_end() const noexcept -> bool
    {
        return position_ >= static_cast<std::uint32_t>(input_.size());
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto match_string(std::string_view str) noexcept -> bool
    {
        if (input_.substr(position_).starts_with(str)) {
            position_ += static_cast<std::uint32_t>(str.size());
            return true;
        }
        return false;
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto peek(std::uint32_t k = 0) const noexcept -> char
    {
        const auto pos = position_ + k;
        if (pos >= static_cast<std::uint32_t>(input_.size())) [[unlikely]] {
            return 0;
        }
        return input_.at(pos);
    }

    __attribute__((always_inline)) auto skip_whitespace() noexcept -> void
    {
        while (!at_end() && is_space(peek())) {
            ++position_;
        }
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto next_token() -> std::expected<Token, Diagnostics>
    {
        while (true) {
            skip_whitespace();

            if (at_end()) [[unlikely]] {
                Token token{
                  .position = position_,
                  .length = 0,
                  .kind = TokenType::END_OF_FILE,
                };

                if (first_leading_comment_pos_.has_value()) {
                    token.leading = *first_leading_comment_pos_ + 1;
                    first_leading_comment_pos_.reset();
                }

                return token;
            }

            Token token;
            switch (peek()) {
                case '{':         token = lex_single_char(TokenType::LEFT_BRACE); break;
                case '}':         token = lex_single_char(TokenType::RIGHT_BRACE); break;
                case '[':         token = lex_single_char(TokenType::LEFT_BRACKET); break;
                case ']':         token = lex_single_char(TokenType::RIGHT_BRACKET); break;
                case '(':         token = lex_single_char(TokenType::LEFT_PAREN); break;
                case ')':         token = lex_single_char(TokenType::RIGHT_PAREN); break;
                case ':':         token = lex_single_char(TokenType::COLON); break;
                case ';':         token = lex_single_char(TokenType::SEMICOLON); break;
                case ',':         token = lex_single_char(TokenType::COMMA); break;
                case '?':         token = lex_single_char(TokenType::QUESTION); break;
                case '$':         token = lex_single_char(TokenType::DOLLAR); break;
                case '.':         token = TRY(lex_dot()); break;
                case '!':         token = TRY(lex_bang()); break;
                case '=':         token = TRY(lex_equal()); break;
                case '<':         token = lex_less(); break;
                case '>':         token = lex_greater(); break;
                case '"':         token = TRY(lex_string()); break;
                case '@':         token = TRY(lex_at()); break;
                case '/':         TRY(lex_comment()); continue;
                case '0' ... '9': token = lex_number(); break;
                default:          token = TRY(lex_identifier_or_keyword()); break;
            }

            if (first_leading_comment_pos_.has_value()) {
                token.leading = *first_leading_comment_pos_ + 1;
                first_leading_comment_pos_.reset();
            }

            return token;
        }
    }

    //===------------------------------------------------------------------===//
    // Lexing Helpers
    //===------------------------------------------------------------------===//

    __attribute__((always_inline)) [[nodiscard]]
    auto lex_at() -> std::expected<Token, Diagnostics>
    {
        static constexpr auto KEYWORDS = std::to_array<std::pair<std::string_view, TokenType>>({
          // Control Flow
          {"@if",       TokenType::AT_IF      },
          {"@else-if",  TokenType::AT_ELSE_IF },
          {"@else",     TokenType::AT_ELSE    },
          {"@for",      TokenType::AT_FOR     },
          {"@break",    TokenType::AT_BREAK   },
          {"@continue", TokenType::AT_CONTINUE},

          // Diagnostic Directives
          {"@error",    TokenType::AT_ERROR   },
          {"@warning",  TokenType::AT_WARNING },
          {"@info",     TokenType::AT_INFO    },
          {"@debug",    TokenType::AT_DEBUG   },
        });

        const auto start_pos = position_;

        for (const auto& [keyword, type] : KEYWORDS) {
            if (match_string(keyword)) {
                return Token{
                  .position = start_pos,
                  .length = position_ - start_pos,
                  .kind = type,
                };
            }
        }

        return error("unexpected character after '@'", start_pos);
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto lex_bang() -> std::expected<Token, Diagnostics>
    {
        const auto start_pos = position_;

        if (match_string("!=")) [[likely]] {
            return Token{
              .position = start_pos,
              .length = position_ - start_pos,
              .kind = TokenType::NOT_EQUAL,
            };
        }

        return error("unexpected character after '!'", start_pos);
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto lex_comment() -> std::expected<void, Diagnostics>
    {
        const auto start_pos = position_;

        // Classify comment type...
        bool is_block = false;

        if (match_string("//")) {
            is_block = false;
        } else if (match_string("/*")) {
            is_block = true;
        } else [[unlikely]] {
            return error("unexpected character after '/'", start_pos);
        }

        // Scan...
        const auto remaining = input_.substr(position_);

        if (is_block) {
            const auto end_marker = remaining.find("*/");

            if (end_marker == std::string_view::npos) [[unlikely]] {
                position_ = static_cast<std::uint32_t>(input_.size());
                return error("unterminated block comment", start_pos);
            }

            position_ += static_cast<std::uint32_t>(end_marker + 2);
        } else {
            const auto newline_pos = remaining.find('\n');

            if (newline_pos != std::string_view::npos) {
                position_ += static_cast<std::uint32_t>(newline_pos);
            } else {
                position_ = static_cast<std::uint32_t>(input_.size());
            }
        }

        // Attach to token stream...
        if (!tokens_.empty()) {
            auto& last_token = tokens_.back();
            const auto last_end = last_token.position + last_token.length;
            const auto gap = input_.substr(last_end, start_pos - last_end);

            if (gap.contains('\n')) {
                if (!first_leading_comment_pos_.has_value()) {
                    first_leading_comment_pos_ = start_pos;
                }
            } else {
                // Since trailing = 0 means that there is no comment, an
                // offset of 1 is used to solve the issue for  comment at
                // position = 0
                last_token.trailing = start_pos + 1;
            }
        } else {
            // Comment is at start of file. Attach to first token or EOF token
            // if file is empty
            if (!first_leading_comment_pos_.has_value()) {
                first_leading_comment_pos_ = start_pos;
            }
        }

        return {};
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto lex_dot() -> std::expected<Token, Diagnostics>
    {
        const auto start_pos = position_;

        if (match_string("..")) [[likely]] {
            return Token{
              .position = start_pos,
              .length = position_ - start_pos,
              .kind = TokenType::RANGE,
            };
        }

        return error("unexpected character after '.'", start_pos);
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto lex_equal() -> std::expected<Token, Diagnostics>
    {
        const auto start_pos = position_;

        if (match_string("==")) [[likely]] {
            return Token{
              .position = start_pos,
              .length = position_ - start_pos,
              .kind = TokenType::EQUAL,
            };
        }

        return error("unexpected character after '='", start_pos);
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto lex_greater() noexcept -> Token
    {
        const auto start_pos = position_;

        if (match_string(">=")) {
            return Token{
              .position = start_pos,
              .length = position_ - start_pos,
              .kind = TokenType::GREATER_EQUAL,
            };
        }

        advance();
        return Token{
          .position = start_pos,
          .length = 1,
          .kind = TokenType::GREATER,
        };
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto lex_less() noexcept -> Token
    {
        const auto start_pos = position_;

        if (match_string("<=")) {
            return Token{
              .position = start_pos,
              .length = position_ - start_pos,
              .kind = TokenType::LESS_EQUAL,
            };
        }

        advance();
        return Token{
          .position = start_pos,
          .length = 1,
          .kind = TokenType::LESS,
        };
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto lex_number() noexcept -> Token
    {
        const auto start_pos = position_;

        while (!at_end() && is_digit(peek())) {
            advance();
        }

        return Token{
          .position = start_pos,
          .length = position_ - start_pos,
          .kind = TokenType::NUMBER,
        };
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto lex_single_char(TokenType type) noexcept -> Token
    {
        const auto start_pos = position_;
        advance();
        return Token{
          .position = start_pos,
          .length = 1,
          .kind = type,
        };
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto lex_string() -> std::expected<Token, Diagnostics>
    {
        const auto start_pos = position_;
        advance(); // consume opening "

        while (peek() != '"') {
            if (at_end()) [[unlikely]] {
                return error("unterminated string literal", start_pos);
            }

            const char c = peek();

            if (c == '\n' || c == '\r') [[unlikely]] {
                return error("unterminated string literal", start_pos);
            }

            if (c == '\\') {
                advance(); // consume '\'
                const char next = peek();
                if (next != '"' && next != 'n' && next != 't' && next != 'r' && next != '\\')
                  [[unlikely]]
                {
                    const auto err_pos = position_ - 1;
                    // recover: consume to end of string
                    while (!at_end() && peek() != '"' && peek() != '\n') {
                        advance();
                    }
                    if (peek() == '"') {
                        advance();
                    }
                    return error(
                      "invalid escape sequence", err_pos, R"(valid escapes: \", \n, \t, \r, \\)");
                }
                advance(); // consume escaped character
            } else {
                advance();
            }
        }

        advance(); // consume closing "

        return Token{
          .position = start_pos,
          .length = position_ - start_pos,
          .kind = TokenType::STRING,
        };
    }

    __attribute__((always_inline)) [[nodiscard]]
    auto lex_identifier_or_keyword() -> std::expected<Token, Diagnostics>
    {
        static constexpr auto KEYWORDS = std::to_array<std::pair<std::string_view, TokenType>>({
          // Top-Level Declarations
          {"project",      TokenType::PROJECT     },
          {"workspace",    TokenType::WORKSPACE   },
          {"target",       TokenType::TARGET      },
          {"dependencies", TokenType::DEPENDENCIES},
          {"options",      TokenType::OPTIONS     },
          {"mixin",        TokenType::MIXIN       },
          {"profile",      TokenType::PROFILE     },
          {"install",      TokenType::INSTALL     },
          {"package",      TokenType::PACKAGE     },
          {"script",       TokenType::SCRIPT      },
          {"with",         TokenType::WITH        },

          // Visibility Modifiers
          {"public",       TokenType::PUBLIC      },
          {"private",      TokenType::PRIVATE     },
          {"interface",    TokenType::INTERFACE   },

          // Control Flow
          {"in",           TokenType::IN          },

          // Logical Operators
          {"and",          TokenType::AND         },
          {"or",           TokenType::OR          },
          {"not",          TokenType::NOT         },

          // Literals
          {"true",         TokenType::TRUE        },
          {"false",        TokenType::FALSE       },
        });

        const auto start_pos = position_;

        while (!at_end() && is_identifier(peek())) {
            advance();
        }

        const auto text = input_.substr(start_pos, position_ - start_pos);

        if (text.empty()) [[unlikely]] {
            return error("unexpected character",
                         position_,
                         "expected an identifier, keyword, or other valid token");
        }

        for (const auto& [keyword, type] : KEYWORDS) {
            if (text == keyword) {
                return Token{
                  .position = start_pos,
                  .length = position_ - start_pos,
                  .kind = type,
                };
            }
        }

        return Token{
          .position = start_pos,
          .length = position_ - start_pos,
          .kind = TokenType::IDENTIFIER,
        };
    }
};

} // namespace kumi::lang

