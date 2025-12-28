/// @file parser_test.cpp
/// @brief Comprehensive unit tests for the Kumi parser

#include "ast/ast.hpp"
#include "lex/lexer.hpp"
#include "parse/parser.hpp"

#include <catch2/catch_all.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <print>

using kumi::ApplyStmt;
using kumi::AST;
using kumi::DependenciesDecl;
using kumi::DiagnosticLevel;
using kumi::DiagnosticStmt;
using kumi::Expression;
using kumi::FeaturesDecl;
using kumi::ForStmt;
using kumi::FunctionCall;
using kumi::GlobalDecl;
using kumi::IfStmt;
using kumi::ImportStmt;
using kumi::InstallDecl;
using kumi::Lexer;
using kumi::LoopControl;
using kumi::LoopControlStmt;
using kumi::MixinDecl;
using kumi::OptionsDecl;
using kumi::PackageDecl;
using kumi::ParseError;
using kumi::Parser;
using kumi::PresetDecl;
using kumi::ProjectDecl;
using kumi::Property;
using kumi::RootDecl;
using kumi::ScriptsDecl;
using kumi::Statement;
using kumi::TargetDecl;
using kumi::TestingDecl;
using kumi::ToolchainDecl;
using kumi::Value;
using kumi::Visibility;
using kumi::VisibilityBlock;
using kumi::WorkspaceDecl;

using Catch::Matchers::ContainsSubstring;

//===---------------------------------------------------------------------===//
// Test Helpers
//===---------------------------------------------------------------------===//

/// @brief Parse input and return AST
static auto parse(std::string_view input) -> std::expected<AST, ParseError>
{
    std::string source{ input };
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        return std::unexpected(tokens.error());
    }

    Parser parser(tokens.value());
    return parser.parse();
}

/// @brief Check if parsing fails
static auto fails(std::string_view input) -> bool
{
    return !parse(input).has_value();
}

//===---------------------------------------------------------------------===//
// Basic Parsing
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: empty input", "[parser][basic]")
{
    auto ast = parse("");
    REQUIRE(ast.has_value());
    CHECK(ast->statements.empty());
}

TEST_CASE("Parser: whitespace and comments", "[parser][basic]")
{
    auto ast = parse(R"(
        // Line comment
        /* Block comment */
        project test { }
    )");
    REQUIRE(ast.has_value());
    CHECK(ast->statements.size() == 1);
}

//===---------------------------------------------------------------------===//
// Root Declaration
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: :root declaration with variables", "[parser][root]")
{
    auto ast = parse(R"(
        :root {
            --std: 23;
            --warn: strict;
            --version: "1.0.0";
        }
    )");
    REQUIRE(ast.has_value());
    REQUIRE(ast->statements.size() == 1);

    const auto *root_decl = std::get_if<RootDecl>(&ast->statements[0]);
    REQUIRE(root_decl != nullptr);
    REQUIRE(root_decl->variables.size() == 3);

    // Check first variable: --std: 23
    CHECK(root_decl->variables[0].key == "--std");
    REQUIRE(root_decl->variables[0].values.size() == 1);
    const auto *std_val = std::get_if<int>(&root_decl->variables[0].values[0]);
    REQUIRE(std_val != nullptr);
    CHECK(*std_val == 23);

    // Check second variable: --warn: strict
    CHECK(root_decl->variables[1].key == "--warn");
    REQUIRE(root_decl->variables[1].values.size() == 1);
    const auto *warn_val = std::get_if<std::string>(&root_decl->variables[1].values[0]);
    REQUIRE(warn_val != nullptr);
    CHECK(*warn_val == "strict");

    // Check third variable: --version: "1.0.0"
    CHECK(root_decl->variables[2].key == "--version");
    REQUIRE(root_decl->variables[2].values.size() == 1);
    const auto *version_val = std::get_if<std::string>(&root_decl->variables[2].values[0]);
    REQUIRE(version_val != nullptr);
    CHECK(*version_val == "1.0.0");
}

//===---------------------------------------------------------------------===//
// Project Declaration
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: project declaration", "[parser][project]")
{
    SECTION("minimal project")
    {
        auto ast = parse("project myapp { }");
        REQUIRE(ast.has_value());
        REQUIRE(ast->statements.size() == 1);

        const auto *project = std::get_if<ProjectDecl>(&ast->statements[0]);
        REQUIRE(project != nullptr);
        CHECK(project->name == "myapp");
        CHECK(project->properties.empty());
    }

    SECTION("project with single property")
    {
        auto ast = parse(R"(
            project myapp {
                version: "1.0.0";
            }
        )");
        REQUIRE(ast.has_value());
        REQUIRE(ast->statements.size() == 1);

        const auto *project = std::get_if<ProjectDecl>(&ast->statements[0]);
        REQUIRE(project != nullptr);
        CHECK(project->name == "myapp");
        REQUIRE(project->properties.size() == 1);
        CHECK(project->properties[0].key == "version");

        const auto *value = std::get_if<std::string>(&project->properties[0].values[0]);
        REQUIRE(value != nullptr);
        CHECK(*value == "1.0.0");
    }

    SECTION("project with multiple properties")
    {
        auto ast = parse(R"(
            project myapp {
                version: "1.0.0";
                language: cpp23;
                license: "MIT";
            }
        )");
        REQUIRE(ast.has_value());

        const auto *project = std::get_if<ProjectDecl>(&ast->statements[0]);
        REQUIRE(project != nullptr);
        CHECK(project->properties.size() == 3);
        CHECK(project->properties[0].key == "version");
        CHECK(project->properties[1].key == "language");
        CHECK(project->properties[2].key == "license");
    }

    SECTION("project name with hyphens")
    {
        auto ast = parse("project my-awesome-app { }");
        REQUIRE(ast.has_value());

        const auto *project = std::get_if<ProjectDecl>(&ast->statements[0]);
        REQUIRE(project != nullptr);
        CHECK(project->name == "my-awesome-app");
    }
}

//===---------------------------------------------------------------------===//
// Target Declaration
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: target declaration", "[parser][target]")
{
    SECTION("minimal target")
    {
        auto ast = parse("target main { }");
        REQUIRE(ast.has_value());
        REQUIRE(ast->statements.size() == 1);

        const auto *target = std::get_if<TargetDecl>(&ast->statements[0]);
        REQUIRE(target != nullptr);
        CHECK(target->name == "main");
        CHECK(target->body.empty());
    }

    SECTION("target with properties")
    {
        auto ast = parse(R"(
            target main {
                type: executable;
                sources: "src/**/*.cpp";
            }
        )");
        REQUIRE(ast.has_value());

        const auto *target = std::get_if<TargetDecl>(&ast->statements[0]);
        REQUIRE(target != nullptr);
        CHECK(target->name == "main");
        REQUIRE(target->body.size() == 2);

        const auto *prop1 = std::get_if<Property>(&target->body[0]);
        REQUIRE(prop1 != nullptr);
        CHECK(prop1->key == "type");

        const auto *prop2 = std::get_if<Property>(&target->body[1]);
        REQUIRE(prop2 != nullptr);
        CHECK(prop2->key == "sources");
    }

    SECTION("target with visibility blocks")
    {
        auto ast = parse(R"(
            target mylib {
                type: static-lib;

                public {
                    include-dirs: "include";
                }

                private {
                    defines: INTERNAL_BUILD;
                }
            }
        )");
        REQUIRE(ast.has_value());

        const auto *target = std::get_if<TargetDecl>(&ast->statements[0]);
        REQUIRE(target != nullptr);
        REQUIRE(target->body.size() == 3);

        const auto *vis1 = std::get_if<VisibilityBlock>(&target->body[1]);
        REQUIRE(vis1 != nullptr);
        CHECK(vis1->visibility == Visibility::PUBLIC);

        const auto *vis2 = std::get_if<VisibilityBlock>(&target->body[2]);
        REQUIRE(vis2 != nullptr);
        CHECK(vis2->visibility == Visibility::PRIVATE);
    }
}

//===---------------------------------------------------------------------===//
// Property Values
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: property values", "[parser][values]")
{
    SECTION("string value")
    {
        auto ast = parse(R"(project test { name: "myapp"; })");
        REQUIRE(ast.has_value());

        const auto *project = std::get_if<ProjectDecl>(&ast->statements[0]);
        const auto *value = std::get_if<std::string>(&project->properties[0].values[0]);
        REQUIRE(value != nullptr);
        CHECK(*value == "myapp");
    }

    SECTION("number value")
    {
        auto ast = parse("project test { count: 42; }");
        REQUIRE(ast.has_value());

        const auto *project = std::get_if<ProjectDecl>(&ast->statements[0]);
        const auto *value = std::get_if<int>(&project->properties[0].values[0]);
        REQUIRE(value != nullptr);
        CHECK(*value == 42);
    }

    // SECTION("negative number")
    // {
    //     // TODO(domi): should fail, so test is false
    //     auto ast = parse("project test { offset: -10; }");
    //     REQUIRE(ast.has_value());

    //     const auto *project = std::get_if<ProjectDecl>(&ast->statements[0]);
    //     const auto *value = std::get_if<int>(&project->properties[0].values[0]);
    //     REQUIRE(value != nullptr);
    //     CHECK(*value == -10);
    // }

    SECTION("boolean true")
    {
        auto ast = parse("project test { enabled: true; }");
        REQUIRE(ast.has_value());

        const auto *project = std::get_if<ProjectDecl>(&ast->statements[0]);
        const auto *value = std::get_if<bool>(&project->properties[0].values[0]);
        REQUIRE(value != nullptr);
        CHECK(*value == true);
    }

    SECTION("boolean false")
    {
        auto ast = parse("project test { enabled: false; }");
        REQUIRE(ast.has_value());

        const auto *project = std::get_if<ProjectDecl>(&ast->statements[0]);
        const auto *value = std::get_if<bool>(&project->properties[0].values[0]);
        REQUIRE(value != nullptr);
        CHECK(*value == false);
    }

    SECTION("identifier value")
    {
        auto ast = parse("project test { type: executable; }");
        REQUIRE(ast.has_value());

        const auto *project = std::get_if<ProjectDecl>(&ast->statements[0]);
        const auto *value = std::get_if<std::string>(&project->properties[0].values[0]);
        REQUIRE(value != nullptr);
        CHECK(*value == "executable");
    }

    SECTION("multiple values (list)")
    {
        auto ast = parse("project test { items: foo, bar, baz; }");
        REQUIRE(ast.has_value());

        const auto *project = std::get_if<ProjectDecl>(&ast->statements[0]);
        REQUIRE(project->properties[0].values.size() == 3);

        const auto *v1 = std::get_if<std::string>(&project->properties[0].values[0]);
        const auto *v2 = std::get_if<std::string>(&project->properties[0].values[1]);
        const auto *v3 = std::get_if<std::string>(&project->properties[0].values[2]);

        REQUIRE(v1 != nullptr);
        REQUIRE(v2 != nullptr);
        REQUIRE(v3 != nullptr);
        CHECK(*v1 == "foo");
        CHECK(*v2 == "bar");
        CHECK(*v3 == "baz");
    }
}

//===---------------------------------------------------------------------===//
// Control Flow: @if
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: @if statement", "[parser][if]")
{
    SECTION("simple @if with function call condition")
    {
        auto ast = parse(R"(
            @if platform(windows) {
                target myapp {
                    defines: PLATFORM_WINDOWS;
                }
            }
        )");
        REQUIRE(ast.has_value());
        REQUIRE(ast->statements.size() == 1);

        const auto *if_stmt = std::get_if<IfStmt>(&ast->statements[0]);
        REQUIRE(if_stmt != nullptr);

        // Check condition is a function call
        const auto *func = std::get_if<FunctionCall>(&if_stmt->condition);
        REQUIRE(func != nullptr);
        CHECK(func->name == "platform");
        REQUIRE(func->arguments.size() == 1);

        // Check then block
        CHECK(if_stmt->then_block.size() == 1);
        CHECK(if_stmt->else_block.empty());
    }

    SECTION("@if with @else")
    {
        auto ast = parse(R"(
            @if config(debug) {
                global { optimize: none; }
            } @else {
                global { optimize: speed; }
            }
        )");
        REQUIRE(ast.has_value());

        const auto *if_stmt = std::get_if<IfStmt>(&ast->statements[0]);
        REQUIRE(if_stmt != nullptr);
        CHECK(if_stmt->then_block.size() == 1);
        CHECK(if_stmt->else_block.size() == 1);
    }

    SECTION("@if with @else-if")
    {
        auto ast = parse(R"(
            @if platform(windows) {
                target app { defines: WIN; }
            } @else @if platform(linux) {
                target app { defines: LINUX; }
            } @else {
                target app { defines: OTHER; }
            }
        )");
        REQUIRE(ast.has_value());

        const auto *if_stmt = std::get_if<IfStmt>(&ast->statements[0]);
        REQUIRE(if_stmt != nullptr);
        CHECK(if_stmt->then_block.size() == 1);
        REQUIRE(if_stmt->else_block.size() == 1);

        // Else block should contain another @if
        const auto *nested_if = std::get_if<IfStmt>(&if_stmt->else_block[0]);
        REQUIRE(nested_if != nullptr);
        CHECK(nested_if->else_block.size() == 1);
    }

    SECTION("@if without body")
    {
        CHECK(fails("@if platform(windows)"));
        CHECK(fails("@if platform(windows) { "));
    }
}

//===---------------------------------------------------------------------===//
// Control Flow: @for
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: @for loop", "[parser][for]")
{
    SECTION("@for with function call iterable")
    {
        auto ast = parse(R"(
            @for file in glob("src/*.cpp") {
                target test { }
            }
        )");
        REQUIRE(ast.has_value());

        const auto *for_stmt = std::get_if<ForStmt>(&ast->statements[0]);
        REQUIRE(for_stmt != nullptr);
        CHECK(for_stmt->variable == "file");

        const auto *func = std::get_if<FunctionCall>(&for_stmt->iterable);
        REQUIRE(func != nullptr);
        CHECK(func->name == "glob");

        CHECK(for_stmt->body.size() == 1);
    }

    SECTION("@for with identifier iterable")
    {
        auto ast = parse(R"(
            @for module in modules {
                target test { }
            }
        )");
        REQUIRE(ast.has_value());

        const auto *for_stmt = std::get_if<ForStmt>(&ast->statements[0]);
        REQUIRE(for_stmt != nullptr);
        CHECK(for_stmt->variable == "module");

        // Iterable should be identifier (stored as string Value)
        const auto *value = std::get_if<Value>(&for_stmt->iterable);
        REQUIRE(value != nullptr);
    }

    SECTION("@for without body")
    {
        CHECK(fails("@for i in list"));
        CHECK(fails("@for i in list {"));
    }

    SECTION("@for missing 'in'")
    {
        CHECK(fails("@for i list { }"));
    }
}

//===---------------------------------------------------------------------===//
// Control Flow: Loop Control
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: loop control statements", "[parser][loop-control]")
{
    SECTION("@break")
    {
        auto ast = parse("@break;");
        REQUIRE(ast.has_value());

        const auto *stmt = std::get_if<LoopControlStmt>(&ast->statements[0]);
        REQUIRE(stmt != nullptr);
        CHECK(stmt->control == LoopControl::BREAK);
    }

    SECTION("@continue")
    {
        auto ast = parse("@continue;");
        REQUIRE(ast.has_value());

        const auto *stmt = std::get_if<LoopControlStmt>(&ast->statements[0]);
        REQUIRE(stmt != nullptr);
        CHECK(stmt->control == LoopControl::CONTINUE);
    }

    SECTION("missing semicolon")
    {
        CHECK(fails("@break"));
        CHECK(fails("@continue"));
    }
}

//===---------------------------------------------------------------------===//
// Diagnostics
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: diagnostic statements", "[parser][diagnostics]")
{
    SECTION("@error")
    {
        auto ast = parse(R"(@error "Build failed";)");
        REQUIRE(ast.has_value());

        const auto *diag = std::get_if<DiagnosticStmt>(&ast->statements[0]);
        REQUIRE(diag != nullptr);
        CHECK(diag->level == DiagnosticLevel::ERROR);
        CHECK(diag->message == "Build failed");
    }

    SECTION("@warning")
    {
        auto ast = parse(R"(@warning "Deprecated";)");
        REQUIRE(ast.has_value());

        const auto *diag = std::get_if<DiagnosticStmt>(&ast->statements[0]);
        REQUIRE(diag != nullptr);
        CHECK(diag->level == DiagnosticLevel::WARNING);
        CHECK(diag->message == "Deprecated");
    }

    SECTION("@info")
    {
        auto ast = parse(R"(@info "Starting build";)");
        REQUIRE(ast.has_value());

        const auto *diag = std::get_if<DiagnosticStmt>(&ast->statements[0]);
        REQUIRE(diag != nullptr);
        CHECK(diag->level == DiagnosticLevel::INFO);
        CHECK(diag->message == "Starting build");
    }

    SECTION("missing string message")
    {
        CHECK(fails("@error;"));
        CHECK(fails("@warning 123;"));
    }
}

//===---------------------------------------------------------------------===//
// Import and Apply
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: @import statement", "[parser][import]")
{
    SECTION("simple import")
    {
        auto ast = parse(R"(@import "common.kumi";)");
        REQUIRE(ast.has_value());

        const auto *import = std::get_if<ImportStmt>(&ast->statements[0]);
        REQUIRE(import != nullptr);
        CHECK(import->path == "common.kumi");
    }

    SECTION("import with path")
    {
        auto ast = parse(R"(@import "configs/base.kumi";)");
        REQUIRE(ast.has_value());

        const auto *import = std::get_if<ImportStmt>(&ast->statements[0]);
        REQUIRE(import != nullptr);
        CHECK(import->path == "configs/base.kumi");
    }

    SECTION("missing semicolon")
    {
        CHECK(fails(R"(@import "file.kumi")"));
    }
}

TEST_CASE("Parser: @apply statement", "[parser][apply]")
{
    SECTION("apply without body")
    {
        auto ast = parse("@apply profile(release);");
        REQUIRE(ast.has_value());

        const auto *apply = std::get_if<ApplyStmt>(&ast->statements[0]);
        REQUIRE(apply != nullptr);
        CHECK(apply->profile.name == "profile");
        REQUIRE(apply->profile.arguments.size() == 1);
        CHECK(apply->body.empty());
    }

    SECTION("apply with body")
    {
        auto ast = parse(R"(
            @apply profile(debug) {
                target myapp {
                    sanitizers: address;
                }
            }
        )");
        REQUIRE(ast.has_value());

        const auto *apply = std::get_if<ApplyStmt>(&ast->statements[0]);
        REQUIRE(apply != nullptr);
        CHECK(apply->profile.name == "profile");
        REQUIRE(apply->body.size() == 1);
    }

    SECTION("apply without function call")
    {
        CHECK(fails("@apply debug { }"));
    }
}

//===---------------------------------------------------------------------===//
// Other Declarations
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: other declarations", "[parser][declarations]")
{
    SECTION("workspace")
    {
        auto ast = parse(R"(
            workspace {
                members: core, renderer, physics;
            }
        )");
        REQUIRE(ast.has_value());

        const auto *workspace = std::get_if<WorkspaceDecl>(&ast->statements[0]);
        REQUIRE(workspace != nullptr);
        REQUIRE(workspace->properties.size() == 1);
        CHECK(workspace->properties[0].key == "members");
    }

    SECTION("dependencies")
    {
        auto ast = parse(R"(
            dependencies {
                fmt: "^10.2.1";
                spdlog: "~1.12.0";
            }
        )");
        REQUIRE(ast.has_value());

        const auto *deps = std::get_if<DependenciesDecl>(&ast->statements[0]);
        REQUIRE(deps != nullptr);
        CHECK(deps->dependencies.size() == 2);
    }

    SECTION("options")
    {
        auto ast = parse(R"(
            options {
                BUILD_TESTS: true;
                ENABLE_LOGGING: false;
            }
        )");
        REQUIRE(ast.has_value());

        const auto *options = std::get_if<OptionsDecl>(&ast->statements[0]);
        REQUIRE(options != nullptr);
        CHECK(options->options.size() == 2);
    }

    SECTION("global")
    {
        auto ast = parse(R"(
            global {
                cpp: 23;
                warnings: all;
            }
        )");
        REQUIRE(ast.has_value());

        const auto *global = std::get_if<GlobalDecl>(&ast->statements[0]);
        REQUIRE(global != nullptr);
        CHECK(global->properties.size() == 2);
    }

    SECTION("mixin")
    {
        auto ast = parse(R"(
            mixin strict-warnings {
                warnings: all;
                warnings-as-errors: true;
            }
        )");
        REQUIRE(ast.has_value());

        const auto *mixin = std::get_if<MixinDecl>(&ast->statements[0]);
        REQUIRE(mixin != nullptr);
        CHECK(mixin->name == "strict-warnings");
        CHECK(mixin->properties.size() == 2);
    }
}

//===---------------------------------------------------------------------===//
// Error Handling
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: error cases", "[parser][errors]")
{
    SECTION("missing closing brace")
    {
        CHECK(fails("project test {"));
        CHECK(fails("target main {"));
    }

    SECTION("missing semicolon")
    {
        CHECK(fails("project test { name: myapp }"));
    }

    SECTION("unexpected token")
    {
        CHECK(fails("project 123 { }"));
        CHECK(fails("target { }"));
    }
}

//===---------------------------------------------------------------------===//
// Integration Tests
//===---------------------------------------------------------------------===//

TEST_CASE("Parser: multiple statements", "[parser][integration]")
{
    auto ast = parse(R"(
        project myapp { version: "1.0.0"; }
        target main { type: executable; }
        dependencies { fmt: "10.2.1"; }
    )");
    REQUIRE(ast.has_value());
    CHECK(ast->statements.size() == 3);
}
