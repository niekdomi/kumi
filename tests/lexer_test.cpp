/// @file lexer_test.cpp
/// @brief Comprehensive unit tests for the lexer
///
/// Tests are organized by token categories matching token.hpp:
/// - Top-Level Declarations
/// - Visibility Modifiers
/// - Control Flow
/// - Diagnostic Directives
/// - Logical Operators
/// - Operators and Punctuation
/// - Literals
/// - Special tokens (EOF, Position Tracking)

#include "lex/lexer.hpp"
#include "lex/token.hpp"

#include <catch2/catch_all.hpp>
#include <string_view>

using kumi::Lexer;
using kumi::ParseError;
using kumi::Token;
using kumi::TokenType;

namespace {

/// Helper: Lex and expect success
auto lex_tokens(std::string_view input) -> std::vector<Token>
{
    Lexer lexer(input);
    auto result = lexer.tokenize();
    REQUIRE(result.has_value());
    return std::move(result.value());
}

/// Helper: Lex single token (input should produce exactly one token + EOF)
auto lex_single(std::string_view input) -> Token
{
    const auto tokens = lex_tokens(input);
    REQUIRE(tokens.size() == 2); // Token + EOF
    return tokens[0];
}

/// Helper: Lex and expect error
auto lex_error(std::string_view input) -> ParseError
{
    Lexer lexer(input);
    const auto result = lexer.tokenize();
    REQUIRE_FALSE(result.has_value());
    return result.error();
}

} // namespace

//===---------------------------------------------------------------------===//
// Top-Level Declarations
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: Top-Level Declarations", "[lexer][top-level]")
{
    SECTION("keywords")
    {
        const auto [input, expected_type] = GENERATE(table<std::string_view, TokenType>({
          {"project",      TokenType::PROJECT     },
          {"workspace",    TokenType::WORKSPACE   },
          {"target",       TokenType::TARGET      },
          {"dependencies", TokenType::DEPENDENCIES},
          {"options",      TokenType::OPTIONS     },
          {"mixin",        TokenType::MIXIN       },
          {"profile",      TokenType::PROFILE     },
          {"@import",      TokenType::AT_IMPORT   },
          {"install",      TokenType::INSTALL     },
          {"package",      TokenType::PACKAGE     },
          {"scripts",      TokenType::SCRIPTS     },
          {"with",         TokenType::WITH        },
        }));
        CAPTURE(input);

        const auto token = lex_single(input);
        CHECK(token.type == expected_type);
        CHECK(token.value == input);
    }

    SECTION("multi-line project declaration")
    {
        const std::string_view input = R"(
            project myapp {
                version: "1.0.0";
            }
        )";

        const auto tokens = lex_tokens(input);

        CHECK(tokens[0].type == TokenType::PROJECT);
        CHECK(tokens[1].type == TokenType::IDENTIFIER);
        CHECK(tokens[1].value == "myapp");
        CHECK(tokens[2].type == TokenType::LEFT_BRACE);

        CHECK(tokens[3].type == TokenType::IDENTIFIER);
        CHECK(tokens[3].value == "version");
        CHECK(tokens[4].type == TokenType::COLON);
        CHECK(tokens[5].type == TokenType::STRING);
        CHECK(tokens[5].value == R"("1.0.0")");
        CHECK(tokens[6].type == TokenType::SEMICOLON);

        CHECK(tokens[7].type == TokenType::RIGHT_BRACE);
        CHECK(tokens[8].type == TokenType::END_OF_FILE);
    }
}

//===---------------------------------------------------------------------===//
// Visibility Modifiers
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: Visibility Modifiers", "[lexer][visibility]")
{
    const auto [input, expected_type] = GENERATE(table<std::string_view, TokenType>({
      {"public",    TokenType::PUBLIC   },
      {"private",   TokenType::PRIVATE  },
      {"interface", TokenType::INTERFACE},
    }));
    CAPTURE(input);

    const auto token = lex_single(input);
    CHECK(token.type == expected_type);
    CHECK(token.value == input);
}

//===---------------------------------------------------------------------===//
// Control Flow
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: Control Flow", "[lexer][control-flow]")
{
    SECTION("keywords")
    {
        const auto [input, expected_type] = GENERATE(table<std::string_view, TokenType>({
          {"@if",       TokenType::AT_IF      },
          {"@else-if",  TokenType::AT_ELSE_IF },
          {"@else",     TokenType::AT_ELSE    },
          {"@for",      TokenType::AT_FOR     },
          {"in",        TokenType::IN         },
          {"@break",    TokenType::AT_BREAK   },
          {"@continue", TokenType::AT_CONTINUE},
        }));
        CAPTURE(input);

        const auto token = lex_single(input);
        CHECK(token.type == expected_type);
        CHECK(token.value == input);
    }

    SECTION("conditional with function call")
    {
        const auto tokens = lex_tokens("@if platform(windows) { }");

        CHECK(tokens[0].type == TokenType::AT_IF);

        CHECK(tokens[1].type == TokenType::IDENTIFIER);
        CHECK(tokens[1].value == "platform");
        CHECK(tokens[2].type == TokenType::LEFT_PAREN);
        CHECK(tokens[3].type == TokenType::IDENTIFIER);
        CHECK(tokens[3].value == "windows");
        CHECK(tokens[4].type == TokenType::RIGHT_PAREN);

        CHECK(tokens[5].type == TokenType::LEFT_BRACE);
        CHECK(tokens[6].type == TokenType::RIGHT_BRACE);

        CHECK(tokens[7].type == TokenType::END_OF_FILE);
    }

    SECTION("for loop with range")
    {
        const auto tokens = lex_tokens("@for worker in 0..7 { }");

        CHECK(tokens[0].type == TokenType::AT_FOR);

        CHECK(tokens[1].type == TokenType::IDENTIFIER);
        CHECK(tokens[1].value == "worker");

        CHECK(tokens[2].type == TokenType::IN);

        CHECK(tokens[3].type == TokenType::NUMBER);
        CHECK(tokens[3].value == "0");
        CHECK(tokens[4].type == TokenType::RANGE);
        CHECK(tokens[4].value == "..");
        CHECK(tokens[5].type == TokenType::NUMBER);
        CHECK(tokens[5].value == "7");

        CHECK(tokens[6].type == TokenType::LEFT_BRACE);
        CHECK(tokens[7].type == TokenType::RIGHT_BRACE);

        CHECK(tokens[8].type == TokenType::END_OF_FILE);
    }

    SECTION("for loop with list")
    {
        const auto tokens = lex_tokens("@for module in [core, renderer, audio] { }");

        CHECK(tokens[0].type == TokenType::AT_FOR);

        CHECK(tokens[1].type == TokenType::IDENTIFIER);
        CHECK(tokens[1].value == "module");

        CHECK(tokens[2].type == TokenType::IN);

        CHECK(tokens[3].type == TokenType::LEFT_BRACKET);
        CHECK(tokens[4].type == TokenType::IDENTIFIER);
        CHECK(tokens[4].value == "core");
        CHECK(tokens[5].type == TokenType::COMMA);
        CHECK(tokens[6].type == TokenType::IDENTIFIER);
        CHECK(tokens[6].value == "renderer");
        CHECK(tokens[7].type == TokenType::COMMA);
        CHECK(tokens[8].type == TokenType::IDENTIFIER);
        CHECK(tokens[8].value == "audio");
        CHECK(tokens[9].type == TokenType::RIGHT_BRACKET);

        CHECK(tokens[10].type == TokenType::LEFT_BRACE);
        CHECK(tokens[11].type == TokenType::RIGHT_BRACE);

        CHECK(tokens[12].type == TokenType::END_OF_FILE);
    }

    SECTION("for loop with function call")
    {
        const auto tokens = lex_tokens(R"(@for file in glob("*.cpp") { })");

        CHECK(tokens[0].type == TokenType::AT_FOR);

        CHECK(tokens[1].type == TokenType::IDENTIFIER);
        CHECK(tokens[1].value == "file");

        CHECK(tokens[2].type == TokenType::IN);

        CHECK(tokens[3].type == TokenType::IDENTIFIER);
        CHECK(tokens[3].value == "glob");
        CHECK(tokens[4].type == TokenType::LEFT_PAREN);
        CHECK(tokens[5].type == TokenType::STRING);
        CHECK(tokens[5].value == R"("*.cpp")");
        CHECK(tokens[6].type == TokenType::RIGHT_PAREN);

        CHECK(tokens[7].type == TokenType::LEFT_BRACE);
        CHECK(tokens[8].type == TokenType::RIGHT_BRACE);

        CHECK(tokens[9].type == TokenType::END_OF_FILE);
    }

    SECTION("invalid @ directives")
    {
        const auto input = GENERATE(as<std::string_view>{},
                                    "@",
                                    "@invalid",
                                    "@123",
                                    "@-if",
                                    "@_else",
                                    "@IF",
                                    "@ELSE",
                                    "@For");
        CAPTURE(input);

        const auto error = lex_error(input);
        CHECK(error.message.contains("unexpected"));
        CHECK(error.help.contains("expected one of"));
    }

    SECTION("invalid @ directive position tracking")
    {
        const std::string_view input = "project myapp @invalid";
        const auto error = lex_error(input);
        CHECK(error.position == 14);
        CHECK(input[error.position] == '@');
        CHECK(error.message.contains("unexpected"));
    }

    SECTION("invalid @ directive with number")
    {
        const std::string_view input = "@if test { } @123 { }";
        const auto error = lex_error(input);
        CHECK(error.position == 13);
        CHECK(input[error.position] == '@');
        CHECK(error.message.contains("unexpected"));
    }

    SECTION("invalid @ directive wrong case")
    {
        const std::string_view input = "target myapp { @IF }";
        const auto error = lex_error(input);
        CHECK(error.position == 15);
        CHECK(input[error.position] == '@');
        CHECK(error.message.contains("unexpected"));
    }
}

//===---------------------------------------------------------------------===//
// Diagnostic Directives
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: Diagnostic Directives", "[lexer][diagnostics]")
{
    const auto [input, expected_type] = GENERATE(table<std::string_view, TokenType>({
      {"@error",   TokenType::AT_ERROR  },
      {"@warning", TokenType::AT_WARNING},
      {"@info",    TokenType::AT_INFO   },
      {"@debug",   TokenType::AT_DEBUG  },
    }));
    CAPTURE(input);

    const auto token = lex_single(input);
    CHECK(token.type == expected_type);
    CHECK(token.value == input);
}

//===---------------------------------------------------------------------===//
// Logical Operators
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: Logical Operators", "[lexer][logical]")
{
    const auto [input, expected_type] = GENERATE(table<std::string_view, TokenType>({
      {"and", TokenType::AND},
      {"or",  TokenType::OR },
      {"not", TokenType::NOT},
    }));
    CAPTURE(input);

    const auto token = lex_single(input);
    CHECK(token.type == expected_type);
    CHECK(token.value == input);
}

//===---------------------------------------------------------------------===//
// Operators and Punctuation
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: Braces, Brackets, and Parentheses", "[lexer][punctuation]")
{
    const auto [input, expected_type] = GENERATE(table<std::string_view, TokenType>({
      {"{", TokenType::LEFT_BRACE   },
      {"}", TokenType::RIGHT_BRACE  },
      {"[", TokenType::LEFT_BRACKET },
      {"]", TokenType::RIGHT_BRACKET},
      {"(", TokenType::LEFT_PAREN   },
      {")", TokenType::RIGHT_PAREN  },
    }));
    CAPTURE(input);

    const auto token = lex_single(input);
    CHECK(token.type == expected_type);
    CHECK(token.value == input);
}

TEST_CASE("Lexer: Delimiters", "[lexer][delimiters]")
{
    SECTION("keywords")
    {
        const auto [input, expected_type] = GENERATE(table<std::string_view, TokenType>({
          {":", TokenType::COLON    },
          {";", TokenType::SEMICOLON},
          {",", TokenType::COMMA    },
        }));
        CAPTURE(input);

        const auto token = lex_single(input);
        CHECK(token.type == expected_type);
        CHECK(token.value == input);
    }

    SECTION("property assignment sequence")
    {
        const auto tokens = lex_tokens(R"(sources: "*.cpp";)");

        CHECK(tokens[0].type == TokenType::IDENTIFIER);
        CHECK(tokens[0].value == "sources");
        CHECK(tokens[1].type == TokenType::COLON);
        CHECK(tokens[2].type == TokenType::STRING);
        CHECK(tokens[2].value == R"("*.cpp")");
        CHECK(tokens[3].type == TokenType::SEMICOLON);
        CHECK(tokens[4].type == TokenType::END_OF_FILE);
    }

    SECTION("comma-separated list values")
    {
        const auto tokens = lex_tokens(R"(authors: "Alice", "Bob", "Charlie";)");

        CHECK(tokens[0].type == TokenType::IDENTIFIER);
        CHECK(tokens[0].value == "authors");
        CHECK(tokens[1].type == TokenType::COLON);

        CHECK(tokens[2].type == TokenType::STRING);
        CHECK(tokens[2].value == R"("Alice")");
        CHECK(tokens[3].type == TokenType::COMMA);

        CHECK(tokens[4].type == TokenType::STRING);
        CHECK(tokens[4].value == R"("Bob")");
        CHECK(tokens[5].type == TokenType::COMMA);

        CHECK(tokens[6].type == TokenType::STRING);
        CHECK(tokens[6].value == R"("Charlie")");
        CHECK(tokens[7].type == TokenType::SEMICOLON);

        CHECK(tokens[8].type == TokenType::END_OF_FILE);
    }
}

TEST_CASE("Lexer: Special Operators", "[lexer][special-operators]")
{
    SECTION("keywords")
    {
        const auto [input, expected_type] = GENERATE(table<std::string_view, TokenType>({
          {"?",  TokenType::QUESTION},
          {"$",  TokenType::DOLLAR  },
          {"..", TokenType::RANGE   },
        }));
        CAPTURE(input);

        const auto token = lex_single(input);
        CHECK(token.type == expected_type);
        CHECK(token.value == input);
    }

    SECTION("range expression")
    {
        const auto tokens = lex_tokens("0..10");

        CHECK(tokens[0].type == TokenType::NUMBER);
        CHECK(tokens[0].value == "0");
        CHECK(tokens[1].type == TokenType::RANGE);
        CHECK(tokens[1].value == "..");
        CHECK(tokens[2].type == TokenType::NUMBER);
        CHECK(tokens[2].value == "10");
        CHECK(tokens[3].type == TokenType::END_OF_FILE);
    }

    SECTION("invalid single dot")
    {
        const std::string_view input = "x . y";
        const auto error = lex_error(input);
        CHECK(error.position == 2);
        CHECK(input[error.position] == '.');
        CHECK(error.message.contains("unexpected"));
        CHECK(error.help.contains("'.'"));
    }
}

TEST_CASE("Lexer: Comparison Operators", "[lexer][comparison]")
{
    SECTION("keywords")
    {
        const auto [input, expected_type] = GENERATE(table<std::string_view, TokenType>({
          {"==", TokenType::EQUAL        },
          {"!=", TokenType::NOT_EQUAL    },
          {"<",  TokenType::LESS         },
          {"<=", TokenType::LESS_EQUAL   },
          {">",  TokenType::GREATER      },
          {">=", TokenType::GREATER_EQUAL},
        }));
        CAPTURE(input);

        const auto token = lex_single(input);
        CHECK(token.type == expected_type);
        CHECK(token.value == input);
    }

    SECTION("adjacent operators without whitespace")
    {
        const auto tokens = lex_tokens("!=>=<===");

        CHECK(tokens[0].type == TokenType::NOT_EQUAL);
        CHECK(tokens[1].type == TokenType::GREATER_EQUAL);
        CHECK(tokens[2].type == TokenType::LESS_EQUAL);
        CHECK(tokens[3].type == TokenType::EQUAL);
        CHECK(tokens[4].type == TokenType::END_OF_FILE);
    }

    SECTION("invalid single equals")
    {
        const std::string_view input = "x = 5";
        const auto error = lex_error(input);
        CHECK(error.position == 2);
        CHECK(input[error.position] == '=');
        CHECK(error.message.contains("unexpected"));
        CHECK(error.help.contains("'='"));
    }

    SECTION("invalid single exclamation")
    {
        const std::string_view input = "x ! y";
        const auto error = lex_error(input);
        CHECK(error.position == 2);
        CHECK(input[error.position] == '!');
        CHECK(error.message.contains("unexpected"));
        CHECK(error.help.contains("'='"));
    }

    SECTION("invalid comment start")
    {
        const std::string_view input = "test / 5";
        const auto error = lex_error(input);
        CHECK(error.position == 5);
        CHECK(input[error.position] == '/');
        CHECK(error.message.contains("unexpected"));
        CHECK(error.help.contains("'/' or '*'"));
    }

    SECTION("position tracking in token sequence")
    {
        const auto tokens = lex_tokens("target myapp {");
        CHECK(tokens[0].position == 0);
        CHECK(tokens[1].position == 7);
        CHECK(tokens[2].position == 13);
    }
}

//===---------------------------------------------------------------------===//
// Literals
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: Identifiers", "[lexer][literals][identifiers]")
{
    SECTION("valid identifiers")
    {
        const auto input = GENERATE(as<std::string_view>{},
                                    "myvar",
                                    "my_var",
                                    "my-var",
                                    "var123",
                                    "_private",
                                    "camelCase",
                                    "PascalCase",
                                    "snake_case",
                                    "SCREAMING_CASE",
                                    "a",
                                    "a1",
                                    "a-b-c",
                                    "a_b_c");
        CAPTURE(input);

        const auto token = lex_single(input);
        CHECK(token.type == TokenType::IDENTIFIER);
        CHECK(token.value == input);
    }

    SECTION("keyword-like identifiers")
    {
        const auto input =
          GENERATE(as<std::string_view>{}, "projects", "targeting", "ands", "trueish", "falsey");
        CAPTURE(input);

        const auto token = lex_single(input);
        CHECK(token.type == TokenType::IDENTIFIER);
        CHECK(token.value == input);
    }

    SECTION("invalid identifier starts")
    {
        const auto input =
          GENERATE(as<std::string_view>{}, "*", "*/", "#", "~", "%", "&", "^", "|", "\\", "`");
        CAPTURE(input);

        const auto error = lex_error(input);
        CHECK(error.message.contains("unexpected character"));
        CHECK(error.help.contains("expected identifier"));
    }
}

TEST_CASE("Lexer: Strings", "[lexer][literals][strings]")
{
    SECTION("simple strings")
    {
        const auto [input, expected_value] = GENERATE(table<std::string_view, std::string_view>({
          {R"("")",            R"("")"           },
          {R"("hello")",       R"("hello")"      },
          {R"("hello world")", R"("hello world")"},
          {R"("123")",         R"("123")"        },
        }));
        CAPTURE(input);

        const auto token = lex_single(input);
        CHECK(token.type == TokenType::STRING);
        CHECK(token.value == expected_value);
    }

    SECTION("escape sequences")
    {
        const auto [input, expected_value] = GENERATE(table<std::string_view, std::string_view>({
          {R"("hello\nworld")",      R"("hello\nworld")"     },
          {R"("hello\tworld")",      R"("hello\tworld")"     },
          {R"("hello\rworld")",      R"("hello\rworld")"     },
          {R"("say \"hello\"")",     R"("say \"hello\"")"    },
          {R"("path\\to\\file")",    R"("path\\to\\file")"   },
          {R"("\\n\\t\\r\\\"\\\\")", R"("\\n\\t\\r\\\"\\\\")"},
        }));
        CAPTURE(input);

        const auto token = lex_single(input);
        CHECK(token.type == TokenType::STRING);
        CHECK(token.value == expected_value);
    }

    SECTION("unterminated string - EOF")
    {
        const auto error = lex_error(R"("unterminated)");
        CHECK(error.message.contains("unterminated"));
        CHECK(error.help.contains("closing"));
    }

    SECTION("unterminated string - newline")
    {
        const auto error = lex_error("\"line1\nline2\"");
        CHECK(error.message.contains("unterminated"));
        CHECK(error.help.contains("multiple lines"));
    }

    SECTION("invalid escape sequences")
    {
        const auto invalid_escape = GENERATE(as<std::string_view>{},
                                             R"("invalid\x")",
                                             R"("invalid\a")",
                                             R"("invalid\b")",
                                             R"("invalid\f")",
                                             R"("invalid\v")",
                                             R"("invalid\0")",
                                             R"("invalid\1")");
        CAPTURE(invalid_escape);

        const auto error = lex_error(invalid_escape);
        CHECK(error.message.contains("invalid escape"));
        CHECK(error.help.contains("valid escapes"));
    }
}

TEST_CASE("Lexer: Numbers", "[lexer][literals][numbers]")
{
    const auto input =
      GENERATE(as<std::string_view>{}, "0", "1", "42", "123", "999999", "1234567890");
    CAPTURE(input);

    const auto token = lex_single(input);
    CHECK(token.type == TokenType::NUMBER);
    CHECK(token.value == input);
}

TEST_CASE("Lexer: Booleans", "[lexer][literals][booleans]")
{
    const auto [input, expected_type] = GENERATE(table<std::string_view, TokenType>({
      {"true",  TokenType::TRUE },
      {"false", TokenType::FALSE},
    }));
    CAPTURE(input);

    const auto token = lex_single(input);
    CHECK(token.type == expected_type);
    CHECK(token.value == input);
}

TEST_CASE("Lexer: Comments", "[lexer][literals][comments]")
{
    SECTION("line comments")
    {
        const auto [input, expected_value] = GENERATE(table<std::string_view, std::string_view>({
          {"//",             "//"            },
          {"// comment",     "// comment"    },
          {"// hello world", "// hello world"},
          {"//no space",     "//no space"    },
          {"// /*",          "// /*"         },
        }));
        CAPTURE(input);

        const auto token = lex_single(input);
        CHECK(token.type == TokenType::COMMENT);
        CHECK(token.value == expected_value);
    }

    SECTION("block comments")
    {
        const auto [input, expected_value] = GENERATE(table<std::string_view, std::string_view>({
          {"/**/",                 "/**/"                },
          {"/* comment */",        "/* comment */"       },
          {"/* multi\nline */",    "/* multi\nline */"   },
          {"/* /* multiple /* */", "/* /* multiple /* */"},
        }));
        CAPTURE(input);

        const auto token = lex_single(input);
        CHECK(token.type == TokenType::COMMENT);
        CHECK(token.value == expected_value);
    }

    SECTION("invalid block comment missing closing */")
    {
        const std::string_view input = "/* unterminated";
        const auto error = lex_error(input);
        CHECK(error.position == 15);
        CHECK(error.position == input.size()); // At EOF
        CHECK(error.message.contains("unterminated"));
        CHECK(error.help.contains("closing */"));
    }

    SECTION("invalid block comment in multiline context")
    {
        const std::string_view input = "project /* unterminated\nstring";
        const auto error = lex_error(input);
        CHECK(error.position == 30);
        CHECK(error.position == input.size()); // At EOF
        CHECK(error.message.contains("unterminated"));
    }

    SECTION("*/ without opening /*")
    {
        const std::string_view input = "project unterminated string */";
        const auto error = lex_error(input);
        CHECK(error.position == 28);
        CHECK(input[error.position] == '*');
        CHECK(error.message.contains("unexpected character"));
    }
}

//===---------------------------------------------------------------------===//
// End of File
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: End of File", "[lexer][eof]")
{
    SECTION("empty input")
    {
        const auto tokens = lex_tokens("");
        REQUIRE(tokens.size() == 1);
        CHECK(tokens[0].type == TokenType::END_OF_FILE);
        CHECK(tokens[0].value == "");
        CHECK(tokens[0].position == 0);
    }

    SECTION("whitespace only")
    {
        const auto tokens = lex_tokens("   \n\t  ");
        REQUIRE(tokens.size() == 1);
        CHECK(tokens[0].type == TokenType::END_OF_FILE);
    }

    SECTION("after tokens")
    {
        const auto tokens = lex_tokens("project myapp");
        REQUIRE(tokens.size() == 3);
        CHECK(tokens[0].position == 0);
        CHECK(tokens[1].position == 8);
        CHECK(tokens[2].type == TokenType::END_OF_FILE);
    }

    SECTION("with leading whitespace")
    {
        const auto tokens = lex_tokens("  target");
        CHECK(tokens[0].position == 2);
    }

    SECTION("with newlines")
    {
        const auto tokens = lex_tokens("target\nproject");
        CHECK(tokens[0].position == 0);
        CHECK(tokens[1].position == 7);
    }
}
