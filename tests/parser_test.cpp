/// @file parser_test.cpp
/// @brief Unit tests for the Kumi parser

#include "ast/ast.hpp"
#include "lex/lexer.hpp"
#include "parse/parser.hpp"

#include <catch2/catch_all.hpp>

using namespace kumi;

//===---------------------------------------------------------------------===//
// Test Helpers
//===---------------------------------------------------------------------===//

/// @brief Helper struct to keep source and tokens alive for AST
struct ParseResult
{
    std::string source;
    std::vector<Token> tokens;
    AST ast;
};

/// @brief Helper to lex and parse input, returning AST on success
/// @note Keeps source and tokens alive to prevent dangling string_views
static auto parse_input(std::string_view input) -> std::expected<ParseResult, ParseError>
{
    std::string source{ input }; // Copy to keep alive
    Lexer lexer(source);
    auto tokens_result = lexer.tokenize();
    if (!tokens_result) {
        return std::unexpected(tokens_result.error());
    }

    Parser parser(tokens_result.value());
    auto ast_result = parser.parse();
    if (!ast_result) {
        return std::unexpected(ast_result.error());
    }

    return ParseResult{
        .source = std::move(source),
        .tokens = std::move(tokens_result.value()),
        .ast = std::move(ast_result.value()),
    };
}

/// @brief Helper to get first statement from parsed input
/// @return Const reference to statement (valid until next call to parse_statement)
static auto parse_statement(std::string_view input)
  -> std::expected<std::reference_wrapper<const Statement>, ParseError>
{
    // Static storage to keep ParseResult alive between calls
    static thread_local std::optional<ParseResult> cached_result;

    auto result = parse_input(input);
    if (!result) {
        return std::unexpected(result.error());
    }

    if (result->ast.statements.empty()) {
        return std::unexpected(ParseError{
          .message = "No statements parsed",
          .position = 0,
        });
    }

    // Cache the result to keep string_views valid
    cached_result = std::move(result.value());
    return std::cref(cached_result->ast.statements[0]);
}

//===---------------------------------------------------------------------===//
// Basic Parsing
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: empty input", "[parser][basic]")
{
    auto result = parse_input("");
    REQUIRE(result.has_value());
    CHECK(result->ast.statements.empty());
}

TEST_CASE("Parser: project declaration", "[parser][declarations]")
{
    SECTION("minimal project")
    {
        auto result = parse_statement("project myapp { }");
        REQUIRE(result.has_value());

        const auto *project = std::get_if<ProjectDecl>(&result.value().get());
        REQUIRE(project != nullptr);
        CHECK(project->name == "myapp");
        CHECK(project->properties.empty());
    }

    SECTION("project with properties")
    {
        auto result = parse_statement("project myapp { version: \"1.0.0\"; }");
        REQUIRE(result.has_value());

        const auto *project = std::get_if<ProjectDecl>(&result.value().get());
        REQUIRE(project != nullptr);
        CHECK(project->name == "myapp");
        REQUIRE(project->properties.size() == 1);
        CHECK(project->properties[0].key == "version");
    }
}

TEST_CASE("Parser: target declaration", "[parser][declarations]")
{
    SECTION("minimal target")
    {
        auto result = parse_statement("target main { }");
        REQUIRE(result.has_value());

        const auto *target = std::get_if<TargetDecl>(&result.value().get());
        REQUIRE(target != nullptr);
        CHECK(target->name == "main");
        CHECK(target->body.empty());
    }

    SECTION("target with properties")
    {
        auto result = parse_statement("target main { kind: executable; }");
        REQUIRE(result.has_value());

        const auto *target = std::get_if<TargetDecl>(&result.value().get());
        REQUIRE(target != nullptr);
        CHECK(target->name == "main");
        REQUIRE(target->body.size() == 1);
    }
}

//===---------------------------------------------------------------------===//
// Properties
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: properties", "[parser][properties]")
{
    SECTION("single value property")
    {
        auto result = parse_statement("project test { name: myapp; }");
        REQUIRE(result.has_value());

        const auto *project = std::get_if<ProjectDecl>(&result.value().get());
        REQUIRE(project != nullptr);
        REQUIRE(project->properties.size() == 1);

        const auto &prop = project->properties[0];
        CHECK(prop.key == "name");
        REQUIRE(prop.values.size() == 1);
    }

    SECTION("multi-value property")
    {
        auto result = parse_statement("project test { items: foo, bar, baz; }");
        REQUIRE(result.has_value());

        const auto *project = std::get_if<ProjectDecl>(&result.value().get());
        REQUIRE(project != nullptr);
        REQUIRE(project->properties.size() == 1);

        const auto &prop = project->properties[0];
        CHECK(prop.key == "items");
        CHECK(prop.values.size() == 3);
    }

    SECTION("multiple properties")
    {
        auto result = parse_statement("project test { name: myapp; version: \"1.0\"; }");
        REQUIRE(result.has_value());

        const auto *project = std::get_if<ProjectDecl>(&result.value().get());
        REQUIRE(project != nullptr);
        CHECK(project->properties.size() == 2);
    }
}

//===---------------------------------------------------------------------===//
// Values
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: value types", "[parser][values]")
{
    SECTION("string value")
    {
        auto result = parse_statement("project test { name: \"myapp\"; }");
        REQUIRE(result.has_value());

        const auto *project = std::get_if<ProjectDecl>(&result.value().get());
        REQUIRE(project != nullptr);
        REQUIRE(project->properties.size() == 1);

        const auto &val = project->properties[0].values[0];
        const auto *str = std::get_if<std::string>(&val);
        REQUIRE(str != nullptr);
        CHECK(*str == "myapp");
    }

    SECTION("number value")
    {
        auto result = parse_statement("project test { count: 42; }");
        REQUIRE(result.has_value());

        const auto *project = std::get_if<ProjectDecl>(&result.value().get());
        REQUIRE(project != nullptr);
        REQUIRE(project->properties.size() == 1);

        const auto &val = project->properties[0].values[0];
        const auto *num = std::get_if<int>(&val);
        REQUIRE(num != nullptr);
        CHECK(*num == 42);
    }

    SECTION("boolean true")
    {
        auto result = parse_statement("project test { enabled: true; }");
        REQUIRE(result.has_value());

        const auto *project = std::get_if<ProjectDecl>(&result.value().get());
        REQUIRE(project != nullptr);
        REQUIRE(project->properties.size() == 1);

        const auto &val = project->properties[0].values[0];
        const auto *boolean = std::get_if<bool>(&val);
        REQUIRE(boolean != nullptr);
        CHECK(*boolean == true);
    }

    SECTION("boolean false")
    {
        auto result = parse_statement("project test { enabled: false; }");
        REQUIRE(result.has_value());

        const auto *project = std::get_if<ProjectDecl>(&result.value().get());
        REQUIRE(project != nullptr);
        REQUIRE(project->properties.size() == 1);

        const auto &val = project->properties[0].values[0];
        const auto *boolean = std::get_if<bool>(&val);
        REQUIRE(boolean != nullptr);
        CHECK(*boolean == false);
    }

    SECTION("identifier value")
    {
        auto result = parse_statement("project test { kind: executable; }");
        REQUIRE(result.has_value());

        const auto *project = std::get_if<ProjectDecl>(&result.value().get());
        REQUIRE(project != nullptr);
        REQUIRE(project->properties.size() == 1);

        const auto &val = project->properties[0].values[0];
        const auto *str = std::get_if<std::string>(&val);
        REQUIRE(str != nullptr);
        CHECK(*str == "executable");
    }
}

//===---------------------------------------------------------------------===//
// Control Flow
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: @if statement", "[parser][control-flow]")
{
    SECTION("simple if")
    {
        // Skip - requires expression parsing
        SKIP("@if requires expression parsing not yet fully implemented");
    }

    SECTION("if with else")
    {
        // Skip - requires expression parsing
        SKIP("@if requires expression parsing not yet fully implemented");
    }
}

TEST_CASE("Parser: @for loop", "[parser][control-flow]")
{
    SECTION("simple for loop")
    {
        // Skip - requires expression parsing
        SKIP("@for requires expression parsing not yet fully implemented");
    }
}

TEST_CASE("Parser: loop control", "[parser][control-flow]")
{
    SECTION("break statement")
    {
        auto result = parse_statement("@break;");
        REQUIRE(result.has_value());

        const auto *loop_ctrl = std::get_if<LoopControlStmt>(&result.value().get());
        REQUIRE(loop_ctrl != nullptr);
        CHECK(loop_ctrl->control == LoopControl::BREAK);
    }

    SECTION("continue statement")
    {
        auto result = parse_statement("@continue;");
        REQUIRE(result.has_value());

        const auto *loop_ctrl = std::get_if<LoopControlStmt>(&result.value().get());
        REQUIRE(loop_ctrl != nullptr);
        CHECK(loop_ctrl->control == LoopControl::CONTINUE);
    }
}

//===---------------------------------------------------------------------===//
// Diagnostics
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: diagnostic statements", "[parser][diagnostics]")
{
    SECTION("error diagnostic")
    {
        auto result = parse_statement("@error \"Something went wrong\";");
        REQUIRE(result.has_value());

        const auto *diag = std::get_if<DiagnosticStmt>(&result.value().get());
        REQUIRE(diag != nullptr);
        CHECK(diag->level == DiagnosticLevel::ERROR);
        CHECK(diag->message == "Something went wrong");
    }

    SECTION("warning diagnostic")
    {
        auto result = parse_statement("@warning \"Deprecated feature\";");
        REQUIRE(result.has_value());

        const auto *diag = std::get_if<DiagnosticStmt>(&result.value().get());
        REQUIRE(diag != nullptr);
        CHECK(diag->level == DiagnosticLevel::WARNING);
    }

    SECTION("info diagnostic")
    {
        auto result = parse_statement("@info \"Build started\";");
        REQUIRE(result.has_value());

        const auto *diag = std::get_if<DiagnosticStmt>(&result.value().get());
        REQUIRE(diag != nullptr);
        CHECK(diag->level == DiagnosticLevel::INFO);
    }
}

//===---------------------------------------------------------------------===//
// Import and Apply
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: @import statement", "[parser][import]")
{
    auto result = parse_statement("@import \"config.kumi\";");
    REQUIRE(result.has_value());

    const auto *import = std::get_if<ImportStmt>(&result.value().get());
    REQUIRE(import != nullptr);
    CHECK(import->path == "config.kumi");
}

TEST_CASE("Parser: @apply statement", "[parser][apply]")
{
    SECTION("apply without body")
    {
        auto result = parse_statement("@apply profile(release);");
        REQUIRE(result.has_value());

        const auto *apply = std::get_if<ApplyStmt>(&result.value().get());
        REQUIRE(apply != nullptr);
        CHECK(apply->profile.name == "profile");
        CHECK(apply->body.empty());
    }

    SECTION("apply with body")
    {
        auto result = parse_statement("@apply profile(debug) { }");
        REQUIRE(result.has_value());

        const auto *apply = std::get_if<ApplyStmt>(&result.value().get());
        REQUIRE(apply != nullptr);
        CHECK(apply->profile.name == "profile");
        CHECK(apply->body.empty());
    }
}

//===---------------------------------------------------------------------===//
// Error Cases
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: error handling", "[parser][errors]")
{
    SECTION("missing closing brace")
    {
        auto result = parse_statement("project test {");
        CHECK(!result.has_value());
    }

    SECTION("missing semicolon")
    {
        auto result = parse_statement("project test { name: myapp }");
        CHECK(!result.has_value());
    }

    SECTION("unexpected token")
    {
        auto result = parse_statement("project 123 { }");
        CHECK(!result.has_value());
    }

    SECTION("invalid property syntax")
    {
        auto result = parse_statement("project test { name }");
        CHECK(!result.has_value());
    }
}

//===---------------------------------------------------------------------===//
// Complex Multi-Statement Inputs
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: multiple statements", "[parser][integration]")
{
    SECTION("multiple projects")
    {
        auto result = parse_input("project app1 { } project app2 { }");
        REQUIRE(result.has_value());
        CHECK(result->ast.statements.size() == 2);
    }

    SECTION("mixed declarations")
    {
        auto result = parse_input(R"(
            project myapp { }
            target main { }
            @import "config.kumi";
        )");
        REQUIRE(result.has_value());
        CHECK(result->ast.statements.size() == 3);
    }
}
