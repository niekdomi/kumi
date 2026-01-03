/// @file parser_test.cpp
/// @brief Comprehensive unit tests for the parser
///
/// Tests are organized by AST node categories matching ast.hpp:
/// - Primitive Values
/// - Expressions (List, Range, FunctionCall, Unary, Comparison, Logical)
/// - Properties
/// - Dependencies
/// - Options
/// - Top-Level Declarations (Project, Workspace, Target, etc.)
/// - Visibility Blocks
/// - Control Flow (If, For, Loop Control)
/// - Diagnostics and Imports
/// - Complete AST

#include "ast/ast.hpp"
#include "lex/lexer.hpp"
#include "parse/parser.hpp"

#include <catch2/catch_all.hpp>
#include <string_view>
#include <variant>

using kumi::AST;
using kumi::ComparisonExpr;
using kumi::ComparisonOperator;
using kumi::Condition;
using kumi::DependenciesDecl;
using kumi::DependencySpec;
using kumi::DependencyValue;
using kumi::DiagnosticLevel;
using kumi::DiagnosticStmt;
using kumi::ForStmt;
using kumi::FunctionCall;
using kumi::IfStmt;
using kumi::ImportStmt;
using kumi::InstallDecl;
using kumi::Iterable;
using kumi::Lexer;
using kumi::List;
using kumi::LogicalExpr;
using kumi::LogicalOperator;
using kumi::LoopControl;
using kumi::LoopControlStmt;
using kumi::MixinDecl;
using kumi::OptionsDecl;
using kumi::OptionSpec;
using kumi::PackageDecl;
using kumi::ParseError;
using kumi::Parser;
using kumi::ProfileDecl;
using kumi::ProjectDecl;
using kumi::Property;
using kumi::Range;
using kumi::ScriptsDecl;
using kumi::Statement;
using kumi::TargetDecl;
using kumi::UnaryExpr;
using kumi::Value;
using kumi::Visibility;
using kumi::VisibilityBlock;
using kumi::WorkspaceDecl;

namespace {

/// Helper: Parse input and expect success
auto parse(std::string_view input) -> AST
{
    Lexer lexer(input);
    auto tokens_result = lexer.tokenize();
    REQUIRE(tokens_result.has_value());

    Parser parser(std::move(tokens_result.value()));
    auto ast_result = parser.parse();
    REQUIRE(ast_result.has_value());

    return std::move(ast_result.value());
}

/// Helper: Parse and expect error
auto parse_error(std::string_view input) -> ParseError
{
    Lexer lexer(input);
    auto tokens_result = lexer.tokenize();
    REQUIRE(tokens_result.has_value());

    Parser parser(std::move(tokens_result.value()));
    auto ast_result = parser.parse();
    REQUIRE_FALSE(ast_result.has_value());

    return ast_result.error();
}

/// Helper: Get first statement of specific type
template<typename T>
auto get_first_statement(const AST& ast) -> const T&
{
    REQUIRE_FALSE(ast.statements.empty());
    REQUIRE(std::holds_alternative<T>(ast.statements[0]));
    return std::get<T>(ast.statements[0]);
}

} // namespace

//===----------------------------------------------------------------------===//
// Primitive Values
//===----------------------------------------------------------------------===//

TEST_CASE("Parser: Values", "[parser][values]")
{
    SECTION("string literal")
    {
        const auto ast = parse(R"(
            project test {
                name: "myapp";
            }
        )");

        const auto& proj = get_first_statement<ProjectDecl>(ast);
        REQUIRE(proj.properties.size() == 1);
        REQUIRE(proj.properties[0].values.size() == 1);
        REQUIRE(std::holds_alternative<std::string>(proj.properties[0].values[0]));
        CHECK(std::get<std::string>(proj.properties[0].values[0]) == "myapp");
    }

    SECTION("integer literal")
    {
        const auto ast = parse(R"(
            project test {
                version: 42;
            }
        )");

        const auto& proj = get_first_statement<ProjectDecl>(ast);
        REQUIRE(proj.properties.size() == 1);
        REQUIRE(proj.properties[0].values.size() == 1);
        REQUIRE(std::holds_alternative<std::uint32_t>(proj.properties[0].values[0]));
        CHECK(std::get<std::uint32_t>(proj.properties[0].values[0]) == 42);
    }

    SECTION("boolean literal - true")
    {
        const auto ast = parse(R"(
            project test {
                enabled: true;
            }
        )");

        const auto& proj = get_first_statement<ProjectDecl>(ast);
        REQUIRE(proj.properties.size() == 1);
        REQUIRE(proj.properties[0].values.size() == 1);
        REQUIRE(std::holds_alternative<bool>(proj.properties[0].values[0]));
        CHECK(std::get<bool>(proj.properties[0].values[0]) == true);
    }

    SECTION("boolean literal - false")
    {
        const auto ast = parse(R"(
            project test {
                enabled: false;
            }
        )");

        const auto& proj = get_first_statement<ProjectDecl>(ast);
        REQUIRE(proj.properties.size() == 1);
        REQUIRE(proj.properties[0].values.size() == 1);
        REQUIRE(std::holds_alternative<bool>(proj.properties[0].values[0]));
        CHECK(std::get<bool>(proj.properties[0].values[0]) == false);
    }

    SECTION("identifier")
    {
        const auto ast = parse(R"(
            project test {
                type: executable;
            }
        )");

        const auto& proj = get_first_statement<ProjectDecl>(ast);
        REQUIRE(proj.properties.size() == 1);
        REQUIRE(proj.properties[0].values.size() == 1);
        REQUIRE(std::holds_alternative<std::string>(proj.properties[0].values[0]));
        CHECK(std::get<std::string>(proj.properties[0].values[0]) == "executable");
    }
}

//===----------------------------------------------------------------------===//
// Expressions
//===----------------------------------------------------------------------===//

TEST_CASE("Parser: List", "[parser][expressions][list]")
{
    SECTION("simple list")
    {
        const auto ast = parse(R"(
            @for item in [a, b, c] {
                target ${item} { }
            }
        )");

        REQUIRE_FALSE(ast.statements.empty());
        REQUIRE(std::holds_alternative<ForStmt>(ast.statements[0]));
        const auto& for_stmt = std::get<ForStmt>(ast.statements[0]);

        REQUIRE(std::holds_alternative<List>(for_stmt.iterable));
        const auto& list = std::get<List>(for_stmt.iterable);

        REQUIRE(list.elements.size() == 3);
        CHECK(std::get<std::string>(list.elements[0]) == "a");
        CHECK(std::get<std::string>(list.elements[1]) == "b");
        CHECK(std::get<std::string>(list.elements[2]) == "c");
    }

    SECTION("list with strings")
    {
        const auto ast = parse(R"(
            @for file in ["main.cpp", "utils.cpp", "helper.cpp"] {
                target test { }
            }
        )");

        const auto& for_stmt = get_first_statement<ForStmt>(ast);
        REQUIRE(std::holds_alternative<List>(for_stmt.iterable));
        const auto& list = std::get<List>(for_stmt.iterable);

        REQUIRE(list.elements.size() == 3);
        CHECK(std::get<std::string>(list.elements[0]) == "main.cpp");
        CHECK(std::get<std::string>(list.elements[1]) == "utils.cpp");
        CHECK(std::get<std::string>(list.elements[2]) == "helper.cpp");
    }
}

TEST_CASE("Parser: Range", "[parser][expressions][range]")
{
    SECTION("simple range")
    {
        const auto ast = parse(R"(
            @for i in 0..10 {
                target test { }
            }
        )");

        const auto& for_stmt = get_first_statement<ForStmt>(ast);
        REQUIRE(std::holds_alternative<Range>(for_stmt.iterable));
        const auto& range = std::get<Range>(for_stmt.iterable);

        CHECK(range.start == 0);
        CHECK(range.end == 10);
    }

    SECTION("range with larger numbers")
    {
        const auto ast = parse(R"(
            @for worker in 1..8 {
                target test { }
            }
        )");

        const auto& for_stmt = get_first_statement<ForStmt>(ast);
        REQUIRE(std::holds_alternative<Range>(for_stmt.iterable));
        const auto& range = std::get<Range>(for_stmt.iterable);

        CHECK(range.start == 1);
        CHECK(range.end == 8);
    }
}

TEST_CASE("Parser: FunctionCall", "[parser][expressions][function]")
{
    SECTION("function with single argument")
    {
        const auto ast = parse(R"(
            @if platform(windows) {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<UnaryExpr>(if_stmt.condition));
        const auto& unary = std::get<UnaryExpr>(if_stmt.condition);

        REQUIRE(std::holds_alternative<FunctionCall>(unary.operand));
        const auto& func = std::get<FunctionCall>(unary.operand);

        CHECK(func.name == "platform");
        REQUIRE(func.arguments.size() == 1);
        CHECK(std::get<std::string>(func.arguments[0]) == "windows");
    }

    SECTION("function with multiple arguments")
    {
        const auto ast = parse(R"(
            @if arch(x86_64, arm64) {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<UnaryExpr>(if_stmt.condition));
        const auto& unary = std::get<UnaryExpr>(if_stmt.condition);

        REQUIRE(std::holds_alternative<FunctionCall>(unary.operand));
        const auto& func = std::get<FunctionCall>(unary.operand);

        CHECK(func.name == "arch");
        REQUIRE(func.arguments.size() == 2);
        CHECK(std::get<std::string>(func.arguments[0]) == "x86_64");
        CHECK(std::get<std::string>(func.arguments[1]) == "arm64");
    }

    SECTION("function in for-loop iterable")
    {
        const auto ast = parse(R"(
            @for file in glob("src/**/*.cpp") {
                target test { }
            }
        )");

        const auto& for_stmt = get_first_statement<ForStmt>(ast);
        REQUIRE(std::holds_alternative<FunctionCall>(for_stmt.iterable));
        const auto& func = std::get<FunctionCall>(for_stmt.iterable);

        CHECK(func.name == "glob");
        REQUIRE(func.arguments.size() == 1);
        CHECK(std::get<std::string>(func.arguments[0]) == "src/**/*.cpp");
    }
}

TEST_CASE("Parser: UnaryExpr", "[parser][expressions][unary]")
{
    SECTION("without negation")
    {
        const auto ast = parse(R"(
            @if platform(linux) {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<UnaryExpr>(if_stmt.condition));
        const auto& unary = std::get<UnaryExpr>(if_stmt.condition);

        CHECK(unary.is_negated == false);
        REQUIRE(std::holds_alternative<FunctionCall>(unary.operand));
    }

    SECTION("with negation")
    {
        const auto ast = parse(R"(
            @if not platform(windows) {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<UnaryExpr>(if_stmt.condition));
        const auto& unary = std::get<UnaryExpr>(if_stmt.condition);

        CHECK(unary.is_negated == true);
        REQUIRE(std::holds_alternative<FunctionCall>(unary.operand));
        const auto& func = std::get<FunctionCall>(unary.operand);
        CHECK(func.name == "platform");
    }
}

TEST_CASE("Parser: ComparisonExpr", "[parser][expressions][comparison]")
{
    SECTION("equal comparison")
    {
        const auto ast = parse(R"(
            @if version == 2 {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<ComparisonExpr>(if_stmt.condition));
        const auto& comp = std::get<ComparisonExpr>(if_stmt.condition);

        REQUIRE(comp.op.has_value());
        CHECK(comp.op.value() == ComparisonOperator::EQUAL);
        REQUIRE(comp.right.has_value());
    }

    SECTION("greater than comparison")
    {
        const auto ast = parse(R"(
            @if threads > 4 {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<ComparisonExpr>(if_stmt.condition));
        const auto& comp = std::get<ComparisonExpr>(if_stmt.condition);

        REQUIRE(comp.op.has_value());
        CHECK(comp.op.value() == ComparisonOperator::GREATER);
    }

    SECTION("unary without comparison")
    {
        const auto ast = parse(R"(
            @if platform(macos) {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        // Should be UnaryExpr, not ComparisonExpr if there's no operator
        REQUIRE(std::holds_alternative<UnaryExpr>(if_stmt.condition));
    }
}

TEST_CASE("Parser: LogicalExpr", "[parser][expressions][logical]")
{
    SECTION("AND operator")
    {
        const auto ast = parse(R"(
            @if platform(windows) and arch(x86_64) {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<LogicalExpr>(if_stmt.condition));
        const auto& logical = std::get<LogicalExpr>(if_stmt.condition);

        CHECK(logical.op == LogicalOperator::AND);
        REQUIRE(logical.operands.size() >= 2);
    }

    SECTION("OR operator")
    {
        const auto ast = parse(R"(
            @if config(debug) or option(FORCE_LOGGING) {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<LogicalExpr>(if_stmt.condition));
        const auto& logical = std::get<LogicalExpr>(if_stmt.condition);

        CHECK(logical.op == LogicalOperator::OR);
        REQUIRE(logical.operands.size() >= 2);
    }

    SECTION("multiple operands")
    {
        const auto ast = parse(R"(
            @if a and b and c {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<LogicalExpr>(if_stmt.condition));
        const auto& logical = std::get<LogicalExpr>(if_stmt.condition);

        CHECK(logical.op == LogicalOperator::AND);
        CHECK(logical.operands.size() == 3);
    }
}

//===----------------------------------------------------------------------===//
// Properties
//===----------------------------------------------------------------------===//

TEST_CASE("Parser: Property", "[parser][property]")
{
    SECTION("single value")
    {
        const auto ast = parse(R"(
            project test {
                type: executable;
            }
        )");

        const auto& proj = get_first_statement<ProjectDecl>(ast);
        REQUIRE(proj.properties.size() == 1);

        const auto& prop = proj.properties[0];
        CHECK(prop.key == "type");
        REQUIRE(prop.values.size() == 1);
        CHECK(std::get<std::string>(prop.values[0]) == "executable");
    }

    SECTION("multiple values")
    {
        const auto ast = parse(R"(
            project test {
                sources: "main.cpp", "utils.cpp", "helper.cpp";
            }
        )");

        const auto& proj = get_first_statement<ProjectDecl>(ast);
        REQUIRE(proj.properties.size() == 1);

        const auto& prop = proj.properties[0];
        CHECK(prop.key == "sources");
        REQUIRE(prop.values.size() == 3);
        CHECK(std::get<std::string>(prop.values[0]) == "main.cpp");
        CHECK(std::get<std::string>(prop.values[1]) == "utils.cpp");
        CHECK(std::get<std::string>(prop.values[2]) == "helper.cpp");
    }

    SECTION("mixed value types")
    {
        const auto ast = parse(R"(
            project test {
                settings: "debug", 42, true;
            }
        )");

        const auto& proj = get_first_statement<ProjectDecl>(ast);
        REQUIRE(proj.properties.size() == 1);

        const auto& prop = proj.properties[0];
        CHECK(prop.key == "settings");
        REQUIRE(prop.values.size() == 3);
        CHECK(std::holds_alternative<std::string>(prop.values[0]));
        CHECK(std::holds_alternative<std::uint32_t>(prop.values[1]));
        CHECK(std::holds_alternative<bool>(prop.values[2]));
    }
}

//===----------------------------------------------------------------------===//
// Dependencies
//===----------------------------------------------------------------------===//

TEST_CASE("Parser: DependencySpec", "[parser][dependencies]")
{
    SECTION("simple version dependency")
    {
        const auto ast = parse(R"(
            dependencies {
                fmt: "10.2.1";
            }
        )");

        const auto& deps = get_first_statement<DependenciesDecl>(ast);
        REQUIRE(deps.dependencies.size() == 1);

        const auto& dep = deps.dependencies[0];
        CHECK(dep.name == "fmt");
        CHECK(dep.is_optional == false);
        REQUIRE(std::holds_alternative<std::string>(dep.value));
        CHECK(std::get<std::string>(dep.value) == "10.2.1");
        CHECK(dep.options.empty());
    }

    SECTION("optional dependency")
    {
        const auto ast = parse(R"(
            dependencies {
                opengl?: system;
            }
        )");

        const auto& deps = get_first_statement<DependenciesDecl>(ast);
        REQUIRE(deps.dependencies.size() == 1);

        const auto& dep = deps.dependencies[0];
        CHECK(dep.name == "opengl");
        CHECK(dep.is_optional == true);
    }

    SECTION("git dependency with options")
    {
        const auto ast = parse(R"(
            dependencies {
                imgui: git("https://github.com/ocornut/imgui") {
                    tag: "v1.90";
                };
            }
        )");

        const auto& deps = get_first_statement<DependenciesDecl>(ast);
        REQUIRE(deps.dependencies.size() == 1);

        const auto& dep = deps.dependencies[0];
        CHECK(dep.name == "imgui");
        CHECK(dep.is_optional == false);
        REQUIRE(std::holds_alternative<FunctionCall>(dep.value));

        const auto& func = std::get<FunctionCall>(dep.value);
        CHECK(func.name == "git");
        REQUIRE(func.arguments.size() == 1);

        REQUIRE(dep.options.size() == 1);
        CHECK(dep.options[0].key == "tag");
    }

    SECTION("multiple dependencies")
    {
        const auto ast = parse(R"(
            dependencies {
                fmt: "10.2.1";
                spdlog: "1.12.0";
                opengl?: system;
            }
        )");

        const auto& deps = get_first_statement<DependenciesDecl>(ast);
        REQUIRE(deps.dependencies.size() == 3);

        CHECK(deps.dependencies[0].name == "fmt");
        CHECK(deps.dependencies[1].name == "spdlog");
        CHECK(deps.dependencies[2].name == "opengl");
        CHECK(deps.dependencies[2].is_optional == true);
    }
}

//===----------------------------------------------------------------------===//
// Options
//===----------------------------------------------------------------------===//

TEST_CASE("Parser: OptionSpec", "[parser][options]")
{
    SECTION("simple boolean option")
    {
        const auto ast = parse(R"(
            options {
                BUILD_TESTS: true;
            }
        )");

        const auto& opts = get_first_statement<OptionsDecl>(ast);
        REQUIRE(opts.options.size() == 1);

        const auto& opt = opts.options[0];
        CHECK(opt.name == "BUILD_TESTS");
        REQUIRE(std::holds_alternative<bool>(opt.default_value));
        CHECK(std::get<bool>(opt.default_value) == true);
        CHECK(opt.constraints.empty());
    }

    SECTION("integer option with constraints")
    {
        const auto ast = parse(R"(
            options {
                MAX_THREADS: 8 {
                    min: 1;
                    max: 128;
                };
            }
        )");

        const auto& opts = get_first_statement<OptionsDecl>(ast);
        REQUIRE(opts.options.size() == 1);

        const auto& opt = opts.options[0];
        CHECK(opt.name == "MAX_THREADS");
        REQUIRE(std::holds_alternative<std::uint32_t>(opt.default_value));
        CHECK(std::get<std::uint32_t>(opt.default_value) == 8);
        REQUIRE(opt.constraints.size() == 2);
        CHECK(opt.constraints[0].key == "min");
        CHECK(opt.constraints[1].key == "max");
    }

    SECTION("string option with choices")
    {
        const auto ast = parse(R"(
            options {
                LOG_LEVEL: "info" {
                    choices: "debug", "info", "warning", "error";
                };
            }
        )");

        const auto& opts = get_first_statement<OptionsDecl>(ast);
        REQUIRE(opts.options.size() == 1);

        const auto& opt = opts.options[0];
        CHECK(opt.name == "LOG_LEVEL");
        REQUIRE(std::holds_alternative<std::string>(opt.default_value));
        CHECK(std::get<std::string>(opt.default_value) == "info");
        REQUIRE(opt.constraints.size() == 1);
        CHECK(opt.constraints[0].key == "choices");
        CHECK(opt.constraints[0].values.size() == 4);
    }
}

//===----------------------------------------------------------------------===//
// Top-Level Declarations
//===----------------------------------------------------------------------===//

TEST_CASE("Parser: ProjectDecl", "[parser][declarations][project]")
{
    SECTION("simple project")
    {
        const auto ast = parse(R"(
            project myapp {
                version: "1.0.0";
            }
        )");

        const auto& proj = get_first_statement<ProjectDecl>(ast);
        CHECK(proj.name == "myapp");
        REQUIRE(proj.properties.size() == 1);
        CHECK(proj.properties[0].key == "version");
    }

    SECTION("project with multiple properties")
    {
        const auto ast = parse(R"(
            project myengine {
                version: "2.0.0";
                cpp: 23;
                license: "MIT";
            }
        )");

        const auto& proj = get_first_statement<ProjectDecl>(ast);
        CHECK(proj.name == "myengine");
        REQUIRE(proj.properties.size() == 3);
        CHECK(proj.properties[0].key == "version");
        CHECK(proj.properties[1].key == "cpp");
        CHECK(proj.properties[2].key == "license");
    }
}

TEST_CASE("Parser: WorkspaceDecl", "[parser][declarations][workspace]")
{
    SECTION("simple workspace")
    {
        const auto ast = parse(R"(
            workspace {
                build-dir: "build";
            }
        )");

        const auto& ws = get_first_statement<WorkspaceDecl>(ast);
        REQUIRE(ws.properties.size() == 1);
        CHECK(ws.properties[0].key == "build-dir");
    }

    SECTION("workspace with multiple properties")
    {
        const auto ast = parse(R"(
            workspace {
                build-dir: "build";
                output-dir: "dist";
                warnings-as-errors: true;
            }
        )");

        const auto& ws = get_first_statement<WorkspaceDecl>(ast);
        REQUIRE(ws.properties.size() == 3);
    }
}

TEST_CASE("Parser: TargetDecl", "[parser][declarations][target]")
{
    SECTION("simple target")
    {
        const auto ast = parse(R"(
            target myapp {
                type: executable;
            }
        )");

        const auto& target = get_first_statement<TargetDecl>(ast);
        CHECK(target.name == "myapp");
        CHECK(target.mixins.empty());
        REQUIRE(target.body.size() == 1);
    }

    SECTION("target with mixins")
    {
        const auto ast = parse(R"(
            target mylib with common-flags, strict-warnings {
                type: static-library;
            }
        )");

        const auto& target = get_first_statement<TargetDecl>(ast);
        CHECK(target.name == "mylib");
        REQUIRE(target.mixins.size() == 2);
        CHECK(target.mixins[0] == "common-flags");
        CHECK(target.mixins[1] == "strict-warnings");
    }

    SECTION("target with properties")
    {
        const auto ast = parse(R"(
            target myapp {
                type: executable;
                sources: "main.cpp";
                cxx-standard: 20;
            }
        )");

        const auto& target = get_first_statement<TargetDecl>(ast);
        CHECK(target.name == "myapp");
        REQUIRE(target.body.size() == 3);
    }
}

TEST_CASE("Parser: DependenciesDecl", "[parser][declarations][dependencies]")
{
    SECTION("simple dependencies block")
    {
        const auto ast = parse(R"(
            dependencies {
                fmt: "10.2.1";
            }
        )");

        const auto& deps = get_first_statement<DependenciesDecl>(ast);
        REQUIRE(deps.dependencies.size() == 1);
        CHECK(deps.dependencies[0].name == "fmt");
    }
}

TEST_CASE("Parser: OptionsDecl", "[parser][declarations][options]")
{
    SECTION("simple options block")
    {
        const auto ast = parse(R"(
            options {
                BUILD_TESTS: true;
            }
        )");

        const auto& opts = get_first_statement<OptionsDecl>(ast);
        REQUIRE(opts.options.size() == 1);
        CHECK(opts.options[0].name == "BUILD_TESTS");
    }
}

TEST_CASE("Parser: MixinDecl", "[parser][declarations][mixin]")
{
    SECTION("simple mixin")
    {
        const auto ast = parse(R"(
            mixin strict-warnings {
                warnings: "all", "extra";
            }
        )");

        const auto& mixin = get_first_statement<MixinDecl>(ast);
        CHECK(mixin.name == "strict-warnings");
        REQUIRE(mixin.body.size() == 1);
    }

    SECTION("mixin with multiple properties")
    {
        const auto ast = parse(R"(
            mixin common-flags {
                warnings: "all";
                optimization: "O2";
                warnings-as-errors: true;
            }
        )");

        const auto& mixin = get_first_statement<MixinDecl>(ast);
        CHECK(mixin.name == "common-flags");
        REQUIRE(mixin.body.size() == 3);
    }
}

TEST_CASE("Parser: ProfileDecl", "[parser][declarations][profile]")
{
    SECTION("simple profile")
    {
        const auto ast = parse(R"(
            profile debug {
                optimize: none;
            }
        )");

        const auto& profile = get_first_statement<ProfileDecl>(ast);
        CHECK(profile.name == "debug");
        CHECK(profile.mixins.empty());
        REQUIRE(profile.properties.size() == 1);
        CHECK(profile.properties[0].key == "optimize");
    }

    SECTION("profile with mixins")
    {
        const auto ast = parse(R"(
            profile release with lto-flags {
                optimize: aggressive;
                debug-info: none;
            }
        )");

        const auto& profile = get_first_statement<ProfileDecl>(ast);
        CHECK(profile.name == "release");
        REQUIRE(profile.mixins.size() == 1);
        CHECK(profile.mixins[0] == "lto-flags");
        REQUIRE(profile.properties.size() == 2);
    }
}

TEST_CASE("Parser: InstallDecl", "[parser][declarations][install]")
{
    SECTION("simple install block")
    {
        const auto ast = parse(R"(
            install {
                destination: "/usr/local/bin";
            }
        )");

        const auto& install = get_first_statement<InstallDecl>(ast);
        REQUIRE(install.properties.size() == 1);
        CHECK(install.properties[0].key == "destination");
    }
}

TEST_CASE("Parser: PackageDecl", "[parser][declarations][package]")
{
    SECTION("simple package block")
    {
        const auto ast = parse(R"(
            package {
                format: "deb";
            }
        )");

        const auto& package = get_first_statement<PackageDecl>(ast);
        REQUIRE(package.properties.size() == 1);
        CHECK(package.properties[0].key == "format");
    }
}

TEST_CASE("Parser: ScriptsDecl", "[parser][declarations][scripts]")
{
    SECTION("simple scripts block")
    {
        const auto ast = parse(R"(
            scripts {
                pre_build: "./setup.sh";
            }
        )");

        const auto& scripts = get_first_statement<ScriptsDecl>(ast);
        REQUIRE(scripts.scripts.size() == 1);
        CHECK(scripts.scripts[0].key == "pre_build");
    }
}

//===----------------------------------------------------------------------===//
// Visibility Blocks
//===----------------------------------------------------------------------===//

TEST_CASE("Parser: VisibilityBlock", "[parser][visibility]")
{
    SECTION("public block")
    {
        const auto ast = parse(R"(
            target mylib {
                public {
                    include-dirs: "include";
                }
            }
        )");

        const auto& target = get_first_statement<TargetDecl>(ast);
        REQUIRE(target.body.size() == 1);
        REQUIRE(std::holds_alternative<VisibilityBlock>(target.body[0]));

        const auto& vis = std::get<VisibilityBlock>(target.body[0]);
        CHECK(vis.visibility == Visibility::PUBLIC);
        REQUIRE(vis.properties.size() == 1);
        CHECK(vis.properties[0].key == "include-dirs");
    }

    SECTION("private block")
    {
        const auto ast = parse(R"(
            target mylib {
                private {
                    sources: "impl.cpp";
                }
            }
        )");

        const auto& target = get_first_statement<TargetDecl>(ast);
        REQUIRE(target.body.size() == 1);
        REQUIRE(std::holds_alternative<VisibilityBlock>(target.body[0]));

        const auto& vis = std::get<VisibilityBlock>(target.body[0]);
        CHECK(vis.visibility == Visibility::PRIVATE);
    }

    SECTION("interface block")
    {
        const auto ast = parse(R"(
            target mylib {
                interface {
                    link-libraries: pthread;
                }
            }
        )");

        const auto& target = get_first_statement<TargetDecl>(ast);
        REQUIRE(target.body.size() == 1);
        REQUIRE(std::holds_alternative<VisibilityBlock>(target.body[0]));

        const auto& vis = std::get<VisibilityBlock>(target.body[0]);
        CHECK(vis.visibility == Visibility::INTERFACE);
    }

    SECTION("multiple visibility blocks")
    {
        const auto ast = parse(R"(
            target mylib {
                public {
                    include-dirs: "include";
                }
                private {
                    sources: "impl.cpp";
                }
            }
        )");

        const auto& target = get_first_statement<TargetDecl>(ast);
        REQUIRE(target.body.size() == 2);
        REQUIRE(std::holds_alternative<VisibilityBlock>(target.body[0]));
        REQUIRE(std::holds_alternative<VisibilityBlock>(target.body[1]));

        const auto& pub = std::get<VisibilityBlock>(target.body[0]);
        const auto& priv = std::get<VisibilityBlock>(target.body[1]);
        CHECK(pub.visibility == Visibility::PUBLIC);
        CHECK(priv.visibility == Visibility::PRIVATE);
    }
}

//===----------------------------------------------------------------------===//
// Control Flow Statements
//===----------------------------------------------------------------------===//

TEST_CASE("Parser: IfStmt", "[parser][control-flow][if]")
{
    SECTION("simple if")
    {
        const auto ast = parse(R"(
            @if platform(windows) {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<UnaryExpr>(if_stmt.condition));
        REQUIRE_FALSE(if_stmt.then_block.empty());
        CHECK(if_stmt.else_block.empty());
    }

    SECTION("if-else")
    {
        const auto ast = parse(R"(
            @if platform(windows) {
                target win { }
            } @else {
                target unix { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE_FALSE(if_stmt.then_block.empty());
        REQUIRE_FALSE(if_stmt.else_block.empty());
    }

    SECTION("if-else-if")
    {
        const auto ast = parse(R"(
            @if platform(windows) {
                target win { }
            } @else-if platform(macos) {
                target mac { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE_FALSE(if_stmt.then_block.empty());
        REQUIRE_FALSE(if_stmt.else_block.empty());

        // else-if is represented as nested IfStmt in else_block
        REQUIRE(std::holds_alternative<IfStmt>(if_stmt.else_block[0]));
    }

    SECTION("if-else-if-else")
    {
        const auto ast = parse(R"(
            @if platform(windows) {
                target win { }
            } @else-if platform(macos) {
                target mac { }
            } @else {
                target linux { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE_FALSE(if_stmt.then_block.empty());
        REQUIRE_FALSE(if_stmt.else_block.empty());

        // else-if is represented as nested IfStmt in else_block
        REQUIRE(std::holds_alternative<IfStmt>(if_stmt.else_block[0]));

        // Check the nested else-if has an else block
        const auto& nested_if = std::get<IfStmt>(if_stmt.else_block[0]);
        REQUIRE_FALSE(nested_if.then_block.empty()); // macos branch
        REQUIRE_FALSE(nested_if.else_block.empty()); // linux (else) branch

        // The final else block should contain a target
        REQUIRE(std::holds_alternative<TargetDecl>(nested_if.else_block[0]));
    }

    SECTION("if with comparison")
    {
        const auto ast = parse(R"(
            @if version > 2 {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<ComparisonExpr>(if_stmt.condition));
    }

    SECTION("if with logical AND")
    {
        const auto ast = parse(R"(
            @if platform(windows) and arch(x86_64) {
                target test { }
            }
        )");

        const auto& if_stmt = get_first_statement<IfStmt>(ast);
        REQUIRE(std::holds_alternative<LogicalExpr>(if_stmt.condition));
        const auto& logical = std::get<LogicalExpr>(if_stmt.condition);
        CHECK(logical.op == LogicalOperator::AND);
    }
}

TEST_CASE("Parser: ForStmt", "[parser][control-flow][for]")
{
    SECTION("for with list")
    {
        const auto ast = parse(R"(
            @for module in [core, renderer, audio] {
                target ${module} { }
            }
        )");

        const auto& for_stmt = get_first_statement<ForStmt>(ast);
        CHECK(for_stmt.variable == "module");
        REQUIRE(std::holds_alternative<List>(for_stmt.iterable));
        REQUIRE_FALSE(for_stmt.body.empty());
    }

    SECTION("for with range")
    {
        const auto ast = parse(R"(
            @for i in 0..8 {
                target worker-${i} { }
            }
        )");

        const auto& for_stmt = get_first_statement<ForStmt>(ast);
        CHECK(for_stmt.variable == "i");
        REQUIRE(std::holds_alternative<Range>(for_stmt.iterable));
    }

    SECTION("for with function call")
    {
        const auto ast = parse(R"(
            @for file in glob("*.cpp") {
                target test { }
            }
        )");

        const auto& for_stmt = get_first_statement<ForStmt>(ast);
        CHECK(for_stmt.variable == "file");
        REQUIRE(std::holds_alternative<FunctionCall>(for_stmt.iterable));
    }
}

TEST_CASE("Parser: LoopControlStmt", "[parser][control-flow][loop-control]")
{
    SECTION("break statement")
    {
        const auto ast = parse(R"(
            @for i in 0..10 {
                @break;
            }
        )");

        const auto& for_stmt = get_first_statement<ForStmt>(ast);
        REQUIRE(for_stmt.body.size() == 1);
        REQUIRE(std::holds_alternative<LoopControlStmt>(for_stmt.body[0]));

        const auto& loop_ctrl = std::get<LoopControlStmt>(for_stmt.body[0]);
        CHECK(loop_ctrl.control == LoopControl::BREAK);
    }

    SECTION("continue statement")
    {
        const auto ast = parse(R"(
            @for i in 0..10 {
                @continue;
            }
        )");

        const auto& for_stmt = get_first_statement<ForStmt>(ast);
        REQUIRE(for_stmt.body.size() == 1);
        REQUIRE(std::holds_alternative<LoopControlStmt>(for_stmt.body[0]));

        const auto& loop_ctrl = std::get<LoopControlStmt>(for_stmt.body[0]);
        CHECK(loop_ctrl.control == LoopControl::CONTINUE);
    }
}

//===----------------------------------------------------------------------===//
// Diagnostic and Import Statements
//===----------------------------------------------------------------------===//

TEST_CASE("Parser: DiagnosticStmt", "[parser][diagnostic]")
{
    SECTION("error statement")
    {
        const auto ast = parse(R"(
            @error "Unsupported platform";
        )");

        const auto& diagnostic = get_first_statement<DiagnosticStmt>(ast);
        CHECK(diagnostic.level == DiagnosticLevel::ERROR);
        CHECK(diagnostic.message == "Unsupported platform");
    }

    SECTION("warning statement")
    {
        const auto ast = parse(R"(
            @warning "Tests are disabled";
        )");

        const auto& diagnostic = get_first_statement<DiagnosticStmt>(ast);
        CHECK(diagnostic.level == DiagnosticLevel::WARNING);
        CHECK(diagnostic.message == "Tests are disabled");
    }

    SECTION("info statement")
    {
        const auto ast = parse(R"(
            @info "Configuring project";
        )");

        const auto& diagnostic = get_first_statement<DiagnosticStmt>(ast);
        CHECK(diagnostic.level == DiagnosticLevel::INFO);
        CHECK(diagnostic.message == "Configuring project");
    }

    SECTION("debug statement")
    {
        const auto ast = parse(R"(
            @debug "Debug information";
        )");

        const auto& diagnostic = get_first_statement<DiagnosticStmt>(ast);
        CHECK(diagnostic.level == DiagnosticLevel::DEBUG);
        CHECK(diagnostic.message == "Debug information");
    }
}

TEST_CASE("Parser: ImportStmt", "[parser][import]")
{
    SECTION("simple import")
    {
        const auto ast = parse(R"(
            @import "common.kumi";
        )");

        const auto& import = get_first_statement<ImportStmt>(ast);
        CHECK(import.path == "common.kumi");
    }

    SECTION("relative import")
    {
        const auto ast = parse(R"(
            @import "../shared/config.kumi";
        )");

        const auto& import = get_first_statement<ImportStmt>(ast);
        CHECK(import.path == "../shared/config.kumi");
    }

    SECTION("windows path import")
    {
        const auto ast = parse(R"(
            @import "C:\\shared\\config.kumi";
        )");

        const auto& import = get_first_statement<ImportStmt>(ast);
        CHECK(import.path == "C:\\shared\\config.kumi");
    }
}

//===----------------------------------------------------------------------===//
// Complete AST
//===----------------------------------------------------------------------===//

TEST_CASE("Parser: Complete AST", "[parser][ast]")
{
    SECTION("multiple top-level statements")
    {
        const auto ast = parse(R"(
            project myapp {
                version: "1.0.0";
            }

            target myapp {
                type: executable;
            }

            dependencies {
                fmt: "10.2.1";
            }
        )");

        REQUIRE(ast.statements.size() == 3);
        CHECK(std::holds_alternative<ProjectDecl>(ast.statements[0]));
        CHECK(std::holds_alternative<TargetDecl>(ast.statements[1]));
        CHECK(std::holds_alternative<DependenciesDecl>(ast.statements[2]));
    }

    SECTION("complex nested structure")
    {
        const auto ast = parse(R"(
            project myengine {
                version: "2.0.0";
            }

            @if platform(windows) {
                target myengine {
                    type: executable;
                    public {
                        include-dirs: "include";
                    }
                    private {
                        sources: "main.cpp";
                    }
                }
            }

            @for module in [core, renderer] {
                target ${module}-lib {
                    type: static-library;
                }
            }
        )");

        REQUIRE(ast.statements.size() == 3);
        CHECK(std::holds_alternative<ProjectDecl>(ast.statements[0]));
        CHECK(std::holds_alternative<IfStmt>(ast.statements[1]));
        CHECK(std::holds_alternative<ForStmt>(ast.statements[2]));
    }

    SECTION("empty file")
    {
        const auto ast = parse("");
        CHECK(ast.statements.empty());
    }
}
