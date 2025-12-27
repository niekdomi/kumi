/// @file lexer_test.cpp
/// @brief Comprehensive unit tests for the Kumi lexer

#include "lex/lexer.hpp"
#include "lex/token.hpp"

#include <catch2/catch_all.hpp>
#include <random>
#include <ranges>
#include <string_view>

using kumi::Lexer;
using kumi::ParseError;
using kumi::Token;
using kumi::TokenType;

namespace {

// Helper: Lex and expect success
auto lex_tokens(std::string_view input) -> std::vector<Token>
{
    Lexer lexer(input);
    auto result = lexer.tokenize();
    REQUIRE(result.has_value());
    return std::move(result.value());
}

// Helper: Lex single token
auto lex_single(std::string_view input) -> Token
{
    const auto tokens = lex_tokens(input);
    REQUIRE(tokens.size() == 2); // Token + EOF
    return tokens[0];
}

// Helper: Lex and expect error
auto lex_error(std::string_view input) -> ParseError
{
    Lexer lexer(input);
    const auto result = lexer.tokenize();
    REQUIRE_FALSE(result.has_value());
    return result.error();
}

// Helper: Generate random identifier with a random length (6 - 21 character long)
auto generate_identifier(std::mt19937 &gen) -> std::string
{
    static constexpr std::string_view START = "abcdefghijklmnopqrstuvwxyz_";
    static constexpr std::string_view CHARS = "abcdefghijklmnopqrstuvwxyz0123456789_-";

    const auto len = std::uniform_int_distribution<>(5, 20)(gen);

    std::string id(1, START[gen() % START.size()]);

    for (auto _ : std::views::iota(0U, static_cast<std::size_t>(len))) {
        id += CHARS[gen() % CHARS.size()];
    }

    return id;
}

} // namespace

//===---------------------------------------------------------------------===//
// At directives (@if, @else-if, @else, @for, @break, @continue, etc.)
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: @ directives", "[lexer][at]")
{
    const auto [input, expected_type, expected_value]
      = GENERATE(table<std::string_view, TokenType, std::string_view>({
        { "@if",       TokenType::IF,             "@if"       },
        { "@else-if",  TokenType::ELSE_IF,        "@else-if"  },
        { "@else",     TokenType::ELSE,           "@else"     },
        { "@for",      TokenType::FOR,            "@for"      },
        { "@break",    TokenType::BREAK,          "@break"    },
        { "@continue", TokenType::CONTINUE,       "@continue" },
        { "@error",    TokenType::ERROR,          "@error"    },
        { "@warning",  TokenType::WARNING,        "@warning"  },
        { "@import",   TokenType::IMPORT_KEYWORD, "@import"   },
        { "@apply",    TokenType::APPLY_KEYWORD,  "@apply"    },
    }));
    CAPTURE(input);

    const auto token = lex_single(input);
    CHECK(token.type == expected_type);
    CHECK(token.value == expected_value);
}

TEST_CASE("Lexer: @ directive errors", "[lexer][at][errors]")
{
    const auto input = GENERATE(as<std::string_view>{}, "@invalid", "@123", "@-if");
    CAPTURE(input);

    const auto error = lex_error(input);
    CHECK_FALSE(error.message.empty());
}

//===---------------------------------------------------------------------===//
// Operators
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: operators", "[lexer][operators]")
{
    const auto [input, expected_type, expected_value]
      = GENERATE(table<std::string_view, TokenType, std::string_view>({
        // Bang operators
        { "!=",  TokenType::NOT_EQUAL,     "!="  },
        { "!",   TokenType::EXCLAMATION,   "!"   },

        // Dot operators
        { "...", TokenType::ELLIPSIS,      "..." },
        { "..",  TokenType::RANGE,         ".."  },
        { ".",   TokenType::DOT,           "."   },

        // Comparison operators
        { "==",  TokenType::EQUAL,         "=="  },
        { "=",   TokenType::ASSIGN,        "="   },
        { ">=",  TokenType::GREATER_EQUAL, ">="  },
        { ">",   TokenType::GREATER,       ">"   },
        { "<=",  TokenType::LESS_EQUAL,    "<="  },
        { "<",   TokenType::LESS,          "<"   },

        // Arrow/minus operators
        { "->",  TokenType::ARROW,         "->"  },
        { "--",  TokenType::MINUS_MINUS,   "--"  },
    }));
    CAPTURE(input);

    const auto token = lex_single(input);
    CHECK(token.type == expected_type);
    CHECK(token.value == expected_value);
}

//===---------------------------------------------------------------------===//
// Literals
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: numbers", "[lexer][literals]")
{
    SECTION("single digit")
    {
        const auto token = lex_single("0");
        CHECK(token.type == TokenType::NUMBER);
        CHECK(token.value == "0");
    }

    SECTION("multiple digits")
    {
        const auto token = lex_single("12345");
        CHECK(token.type == TokenType::NUMBER);
        CHECK(token.value == "12345");
    }

    SECTION("large number")
    {
        const auto token = lex_single("999999");
        CHECK(token.type == TokenType::NUMBER);
        CHECK(token.value == "999999");
    }
}

TEST_CASE("Lexer: strings", "[lexer][literals]")
{
    SECTION("simple string")
    {
        const auto token = lex_single(R"("hello")");
        CHECK(token.type == TokenType::STRING);
        CHECK(token.value == "hello");
    }

    SECTION("empty string")
    {
        const auto token = lex_single(R"("")");
        CHECK(token.type == TokenType::STRING);
        CHECK(token.value == "");
    }

    SECTION("string with spaces")
    {
        const auto token = lex_single(R"("hello world")");
        CHECK(token.type == TokenType::STRING);
        CHECK(token.value == "hello world");
    }

    SECTION("escape sequences")
    {
        const auto token = lex_single(R"("hello\nworld")");
        CHECK(token.type == TokenType::STRING);
        CHECK(token.value == "hello\nworld");
    }

    SECTION("escaped quote")
    {
        const auto token = lex_single(R"("say \"hello\"")");
        CHECK(token.type == TokenType::STRING);
        CHECK(token.value == "say \"hello\"");
    }

    SECTION("escaped backslash")
    {
        const auto token = lex_single(R"("path\\to\\file")");
        CHECK(token.type == TokenType::STRING);
        CHECK(token.value == "path\\to\\file");
    }

    SECTION("tab escape")
    {
        const auto token = lex_single(R"("hello\tworld")");
        CHECK(token.type == TokenType::STRING);
        CHECK(token.value == "hello\tworld");
    }

    SECTION("unterminated string - EOF")
    {
        const auto error = lex_error(R"("unterminated)");
        CHECK(error.message.contains("unterminated"));
    }

    SECTION("unterminated string - newline")
    {
        const auto error = lex_error("\"line1\nline2\"");
        CHECK(error.message.contains("unterminated"));
    }

    SECTION("invalid escape sequence")
    {
        const auto error = lex_error(R"("invalid\x")");
        CHECK(error.message.contains("invalid escape"));
    }
}

//===---------------------------------------------------------------------===//
// Keywords
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: keywords", "[lexer][keywords]")
{
    const auto [input, expected_type, expected_value]
      = GENERATE(table<std::string_view, TokenType, std::string_view>({
        // Keywords - Top level
        { "project",      TokenType::PROJECT,      "project"      },
        { "workspace",    TokenType::WORKSPACE,    "workspace"    },
        { "target",       TokenType::TARGET,       "target"       },
        { "dependencies", TokenType::DEPENDENCIES, "dependencies" },
        { "options",      TokenType::OPTIONS,      "options"      },
        { "global",       TokenType::GLOBAL,       "global"       },
        { "mixin",        TokenType::MIXIN,        "mixin"        },
        { "preset",       TokenType::PRESET,       "preset"       },
        { "features",     TokenType::FEATURES,     "features"     },
        { "testing",      TokenType::TESTING,      "testing"      },
        { "install",      TokenType::INSTALL,      "install"      },
        { "package",      TokenType::PACKAGE,      "package"      },
        { "scripts",      TokenType::SCRIPTS,      "scripts"      },
        { "toolchain",    TokenType::TOOLCHAIN,    "toolchain"    },
        { "root",         TokenType::ROOT,         "root"         },
        { "in",           TokenType::IN,           "in"           },

        // Keywords - Visibility
        { "public",       TokenType::PUBLIC,       "public"       },
        { "private",      TokenType::PRIVATE,      "private"      },
        { "interface",    TokenType::INTERFACE,    "interface"    },

        // Keywords - Properties
        { "type",         TokenType::TYPE,         "type"         },
        { "sources",      TokenType::SOURCES,      "sources"      },
        { "headers",      TokenType::HEADERS,      "headers"      },
        { "depends",      TokenType::DEPENDS,      "depends"      },
        { "apply",        TokenType::APPLY,        "apply"        },
        { "inherits",     TokenType::INHERITS,     "inherits"     },
        { "extends",      TokenType::EXTENDS,      "extends"      },
        { "export",       TokenType::EXPORT,       "export"       },
        { "import",       TokenType::IMPORT,       "import"       },

        // Keywords - Logical
        { "and",          TokenType::AND,          "and"          },
        { "or",           TokenType::OR,           "or"           },
        { "not",          TokenType::NOT,          "not"          },

        // Keywords - Functions
        { "platform",     TokenType::PLATFORM,     "platform"     },
        { "arch",         TokenType::ARCH,         "arch"         },
        { "compiler",     TokenType::COMPILER,     "compiler"     },
        { "config",       TokenType::CONFIG,       "config"       },
        { "option",       TokenType::OPTION,       "option"       },
        { "feature",      TokenType::FEATURE,      "feature"      },
        { "has-feature",  TokenType::HAS_FEATURE,  "has-feature"  },
        { "exists",       TokenType::EXISTS,       "exists"       },
        { "var",          TokenType::VAR,          "var"          },
        { "glob",         TokenType::GLOB,         "glob"         },
        { "git",          TokenType::GIT,          "git"          },
        { "url",          TokenType::URL,          "url"          },
        { "system",       TokenType::SYSTEM,       "system"       },
    }));
    CAPTURE(input);

    const auto token = lex_single(input);
    CHECK(token.type == expected_type);
    CHECK(token.value == expected_value);
}

//===---------------------------------------------------------------------===//
// Identifiers
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: identifiers - common patterns", "[lexer][identifiers]")
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
                                "a");
    CAPTURE(input);

    const auto token = lex_single(input);
    CHECK(token.type == TokenType::IDENTIFIER);
    CHECK(token.value == input);
}

TEST_CASE("Lexer: identifiers - random", "[lexer][identifiers]")
{
    const auto seed = GENERATE(take(20, random(1000, 9999)));
    CAPTURE(seed);

    std::mt19937 gen(static_cast<std::mt19937::result_type>(seed));

    const auto id = generate_identifier(gen);
    const auto token = lex_single(id);
    CHECK(token.type == TokenType::IDENTIFIER);
}

//===---------------------------------------------------------------------===//
// Punctuation
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: single-character punctuation", "[lexer][punctuation]")
{
    const auto [input, expected_type, expected_value]
      = GENERATE(table<std::string_view, TokenType, std::string_view>({
        { "{", TokenType::LEFT_BRACE,    "{" },
        { "}", TokenType::RIGHT_BRACE,   "}" },
        { "[", TokenType::LEFT_BRACKET,  "[" },
        { "]", TokenType::RIGHT_BRACKET, "]" },
        { "(", TokenType::LEFT_PAREN,    "(" },
        { ")", TokenType::RIGHT_PAREN,   ")" },
        { ":", TokenType::COLON,         ":" },
        { ";", TokenType::SEMICOLON,     ";" },
        { ",", TokenType::COMMA,         "," },
        { "?", TokenType::QUESTION,      "?" },
        { "$", TokenType::DOLLAR,        "$" },
    }));
    CAPTURE(input);

    const auto token = lex_single(input);
    CHECK(token.type == expected_type);
    CHECK(token.value == expected_value);
}

//===---------------------------------------------------------------------===//
// Whitespace and Position Tracking
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: whitespace handling", "[lexer][whitespace]")
{
    SECTION("spaces ignored")
    {
        const auto tokens = lex_tokens("  target  ");
        CHECK(tokens[0].position == 2);
    }

    SECTION("tabs ignored")
    {
        const auto tokens = lex_tokens("\ttarget\t");
        CHECK(tokens[0].position == 1);
    }

    SECTION("position tracking for newlines")
    {
        const auto tokens = lex_tokens("target\nproject");
        CHECK(tokens[0].position == 0);
        CHECK(tokens[1].position == 7);
    }

    SECTION("position tracking for multiple tokens")
    {
        const auto tokens = lex_tokens("target myapp {");
        CHECK(tokens[0].position == 0);
        CHECK(tokens[1].position == 7);
        CHECK(tokens[2].position == 13);
    }
}

//===---------------------------------------------------------------------===//
// Complex Multi - Token Inputs
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: multi-token sequences", "[lexer][integration]")
{
    SECTION("simple target definition")
    {
        const auto tokens = lex_tokens("target myapp { }");
        REQUIRE(tokens.size() == 5);
        CHECK(tokens[0].type == TokenType::TARGET);
        CHECK(tokens[1].type == TokenType::IDENTIFIER);
        CHECK(tokens[2].type == TokenType::LEFT_BRACE);
        CHECK(tokens[3].type == TokenType::RIGHT_BRACE);
        CHECK(tokens[4].type == TokenType::END_OF_FILE);
    }

    SECTION("property assignment")
    {
        const auto tokens = lex_tokens("type: executable;");
        REQUIRE(tokens.size() == 5);
        CHECK(tokens[0].type == TokenType::TYPE);
        CHECK(tokens[1].type == TokenType::COLON);
        CHECK(tokens[2].type == TokenType::IDENTIFIER);
        CHECK(tokens[3].type == TokenType::SEMICOLON);
    }

    SECTION("string property")
    {
        const auto tokens = lex_tokens(R"(sources: "*.cpp";)");
        CHECK(tokens[0].type == TokenType::SOURCES);
        CHECK(tokens[1].type == TokenType::COLON);
        CHECK(tokens[2].type == TokenType::STRING);
        CHECK(tokens[2].value == "*.cpp");
        CHECK(tokens[3].type == TokenType::SEMICOLON);
    }

    SECTION("list of dependencies")
    {
        const auto tokens = lex_tokens("depends: fmt, spdlog;");
        CHECK(tokens[0].type == TokenType::DEPENDS);
        CHECK(tokens[1].type == TokenType::COLON);
        CHECK(tokens[2].type == TokenType::IDENTIFIER);
        CHECK(tokens[2].value == "fmt");
        CHECK(tokens[3].type == TokenType::COMMA);
        CHECK(tokens[4].type == TokenType::IDENTIFIER);
        CHECK(tokens[4].value == "spdlog");
        CHECK(tokens[5].type == TokenType::SEMICOLON);
    }

    SECTION("conditional block")
    {
        const auto tokens = lex_tokens("@if platform(windows) { }");
        CHECK(tokens[0].type == TokenType::IF);
        CHECK(tokens[1].type == TokenType::PLATFORM);
        CHECK(tokens[2].type == TokenType::LEFT_PAREN);
        CHECK(tokens[3].type == TokenType::IDENTIFIER);
        CHECK(tokens[3].value == "windows");
        CHECK(tokens[4].type == TokenType::RIGHT_PAREN);
        CHECK(tokens[5].type == TokenType::LEFT_BRACE);
        CHECK(tokens[6].type == TokenType::RIGHT_BRACE);
    }

    SECTION("range expression")
    {
        const auto tokens = lex_tokens("0..10");
        CHECK(tokens[0].type == TokenType::NUMBER);
        CHECK(tokens[1].type == TokenType::RANGE);
        CHECK(tokens[2].type == TokenType::NUMBER);
    }

    SECTION("comparison")
    {
        const auto tokens = lex_tokens("version >= 23");
        CHECK(tokens[0].type == TokenType::IDENTIFIER);
        CHECK(tokens[1].type == TokenType::GREATER_EQUAL);
        CHECK(tokens[2].type == TokenType::NUMBER);
    }
}

//===---------------------------------------------------------------------===//
// Real-World Kumi Examples
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: realistic Kumi code", "[lexer][integration]")
{
    SECTION("project declaration")
    {
        constexpr auto INPUT = R"(
            project myapp {
                version: "1.0.0";
            }
        )";
        const auto tokens = lex_tokens(INPUT);

        CHECK(tokens[0].type == TokenType::PROJECT);
        CHECK(tokens[1].type == TokenType::IDENTIFIER);
        CHECK(tokens[1].value == "myapp");
        CHECK(tokens[2].type == TokenType::LEFT_BRACE);

        CHECK(tokens[3].type == TokenType::IDENTIFIER);
        CHECK(tokens[3].value == "version");
        CHECK(tokens[4].type == TokenType::COLON);
        CHECK(tokens[5].type == TokenType::STRING);
        CHECK(tokens[5].value == "1.0.0");
        CHECK(tokens[6].type == TokenType::SEMICOLON);

        CHECK(tokens[7].type == TokenType::RIGHT_BRACE);
    }

    SECTION("target with dependencies")
    {
        constexpr auto INPUT = R"(
            target myapp {
                type: executable;
                sources: "src/*.cpp";
                depends: fmt, spdlog;
            }
        )";
        const auto tokens = lex_tokens(INPUT);

        CHECK(tokens[0].type == TokenType::TARGET);
        CHECK(tokens[1].type == TokenType::IDENTIFIER);
        CHECK(tokens[1].value == "myapp");
        CHECK(tokens[2].type == TokenType::LEFT_BRACE);

        CHECK(tokens[3].type == TokenType::TYPE);
        CHECK(tokens[4].type == TokenType::COLON);
        CHECK(tokens[5].type == TokenType::IDENTIFIER);
        CHECK(tokens[5].value == "executable");
        CHECK(tokens[6].type == TokenType::SEMICOLON);

        CHECK(tokens[7].type == TokenType::SOURCES);
        CHECK(tokens[8].type == TokenType::COLON);
        CHECK(tokens[9].type == TokenType::STRING);
        CHECK(tokens[9].value == "src/*.cpp");
        CHECK(tokens[10].type == TokenType::SEMICOLON);

        CHECK(tokens[11].type == TokenType::DEPENDS);
        CHECK(tokens[12].type == TokenType::COLON);
        CHECK(tokens[13].type == TokenType::IDENTIFIER);
        CHECK(tokens[13].value == "fmt");
        CHECK(tokens[14].type == TokenType::COMMA);
        CHECK(tokens[15].type == TokenType::IDENTIFIER);
        CHECK(tokens[15].value == "spdlog");
        CHECK(tokens[16].type == TokenType::SEMICOLON);

        CHECK(tokens[17].type == TokenType::RIGHT_BRACE);
    }
}

//===---------------------------------------------------------------------===//
// Edge Cases and Error Conditions
//===---------------------------------------------------------------------===//

TEST_CASE("Lexer: edge cases", "[lexer][edge]")
{
    SECTION("empty input")
    {
        const auto [input, expected_value] = GENERATE(table<std::string_view, std::string_view>({
          { "",          "" },
          { "   \n\t  ", "" },
          { "\n",        "" },
          { "\n\r",      "" },
          { "\r",        "" },
        }));
        CAPTURE(input);

        const auto tokens = lex_tokens(input);
        REQUIRE(tokens.size() == 1);
        CHECK(tokens[0].type == TokenType::END_OF_FILE);
        CHECK(tokens[0].value == expected_value);
    }

    SECTION("adjacent operators")
    {
        auto tokens = lex_tokens("!=>=<=");
        CHECK(tokens[0].type == TokenType::NOT_EQUAL);
        CHECK(tokens[1].type == TokenType::GREATER_EQUAL);
        CHECK(tokens[2].type == TokenType::LESS_EQUAL);
    }

    SECTION("identifier followed by number")
    {
        auto tokens = lex_tokens("var123");
        CHECK(tokens[0].type == TokenType::IDENTIFIER);
        CHECK(tokens[0].value == "var123");
    }

    SECTION("keyword-like identifier")
    {
        auto tokens = lex_tokens("projects");
        CHECK(tokens[0].type == TokenType::IDENTIFIER);
    }
}

TEST_CASE("Lexer: match_string edge cases", "[lexer][edge-cases]")
{
    SECTION("partial keyword at EOF")
    {
        const auto error = lex_error("@els");
        CHECK(error.message.contains("unexpected"));
    }

    SECTION("exact keyword at EOF")
    {
        const auto tokens = lex_tokens("@else");
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].type == TokenType::ELSE);
    }

    SECTION("@ at EOF")
    {
        const auto error = lex_error("@");
        CHECK(error.message.contains("unexpected"));
    }

    SECTION("two-char operator at EOF")
    {
        const auto tokens = lex_tokens("target myapp ==");
        CHECK(tokens[2].type == TokenType::EQUAL);
        CHECK(tokens[2].value == "==");
    }

    SECTION("ellipsis at EOF")
    {
        const auto tokens = lex_tokens("...");
        CHECK(tokens[0].type == TokenType::ELLIPSIS);
        CHECK(tokens[0].value == "...");
    }

    SECTION("line comment at EOF")
    {
        const auto tokens = lex_tokens("target // comment");
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].type == TokenType::TARGET);
    }

    SECTION("unclosed block comment at EOF")
    {
        const auto tokens = lex_tokens("target /* comment");
        REQUIRE(tokens.size() == 2);
        CHECK(tokens[0].type == TokenType::TARGET);
    }

    SECTION("substr safety - position at end")
    {
        const auto tokens = lex_tokens("target");
        CHECK(tokens[0].type == TokenType::TARGET);
        CHECK(tokens[1].type == TokenType::END_OF_FILE);
    }
}
