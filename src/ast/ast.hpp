/// @file ast.hpp
/// @brief Abstract Syntax Tree node definitions
///
/// Defines all AST node types representing parsed Kumi build files.
///
/// @see Parser for AST construction
/// @see Lexer for tokenization

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace kumi {

/// Forward Declarations
struct ApplyStmt;
struct ComparisonExpr;
struct DependenciesDecl;
struct DiagnosticStmt;
struct FeaturesDecl;
struct ForStmt;
struct FunctionCall;
struct GlobalDecl;
struct IfStmt;
struct ImportStmt;
struct InstallDecl;
struct LogicalExpr;
struct LoopControlStmt;
struct MixinDecl;
struct OptionsDecl;
struct PackageDecl;
struct PresetDecl;
struct ProjectDecl;
struct RootDecl;
struct ScriptsDecl;
struct TargetDecl;
struct TestingDecl;
struct ToolchainDecl;
struct Variable;
struct VisibilityBlock;
struct WorkspaceDecl;

//===---------------------------------------------------------------------===//
// Base Node
//===---------------------------------------------------------------------===//

/// @brief Base class for all AST nodes, providing source location information
///
/// All AST node types inherit from this base to track where they appear
/// in the source file for error reporting and debugging.
struct NodeBase
{
    std::size_t line;   ///< Line number in source file
    std::size_t column; ///< Column number in source file
};

//===---------------------------------------------------------------------===//
// Values and Expressions
//===---------------------------------------------------------------------===//

/// @brief Represents a literal value in the AST
///
/// Values can be strings, integers, or booleans. They appear in property
/// assignments, function arguments, and expressions.
///
/// Example:
/// ```css
/// name: "kumi";         // String value
/// version: 42;          // Integer value
/// enabled: true;        // Boolean value
/// ```
using Value = std::variant<std::string, // String literal: "hello"
                           int,         // Number literal: 123
                           bool>;       // Boolean literal: true/false

/// @brief Expression variant representing any evaluable expression
///
/// Expressions can appear in conditions (@if), loop iterables (@for),
/// and property values. They combine literals, variables, function calls,
/// and operators.
using Expression = std::variant<FunctionCall, Variable, ComparisonExpr, LogicalExpr, Value>;

/// @brief Represents a function call expression
///
/// Function calls are used for platform detection, file globbing, and other
/// build-time queries.
///
/// Example:
/// ```css
/// @if platform(windows) { ... }
/// sources: glob("src/*.cpp");
/// ```
struct FunctionCall final : NodeBase
{
    std::string name;             ///< Function name (e.g., "platform", "glob")
    std::vector<Value> arguments; ///< Function arguments
};

/// @brief Represents a variable reference
///
/// Variables reference custom properties defined in :root blocks or
/// command-line options.
///
/// Example:
/// ```css
/// version: var(--app-version);
/// name: ${--project-name};
/// ```
struct Variable final : NodeBase
{
    std::string name; ///< Variable name without the -- prefix
};

/// @brief Comparison operators for conditional expressions
enum class ComparisonOp : std::uint8_t
{
    EQUAL,        ///< ==
    NOT_EQUAL,    ///< !=
    LESS,         ///< <
    LESS_EQUAL,   ///< <=
    GREATER,      ///< >
    GREATER_EQUAL ///< >=
};

/// @brief Represents a comparison expression
///
/// Comparison expressions compare two values or expressions using
/// relational operators.
///
/// Example:
/// ```css
/// @if option(BUILD_TESTS) == true { ... }
/// @if var(--version) >= 2 { ... }
/// ```
struct ComparisonExpr final : NodeBase
{
    ComparisonOp op;                   ///< Comparison operator
    std::unique_ptr<Expression> left;  ///< Left-hand side expression
    std::unique_ptr<Expression> right; ///< Right-hand side expression
};

/// @brief Logical operators for boolean expressions
enum class LogicalOp : std::uint8_t
{
    AND, ///< `and`
    OR,  ///< `or`
    NOT  ///< `not`
};

/// @brief Represents a logical expression
///
/// Logical expressions combine boolean values or expressions using
/// AND, OR, and NOT operators.
///
/// Example:
/// ```css
/// @if platform(windows) and arch(x86_64) { ... }
/// @if not option(DISABLE_TESTS) { ... }
/// ```
struct LogicalExpr final : NodeBase
{
    LogicalOp op;                      ///< Logical operator
    std::unique_ptr<Expression> left;  ///< Left-hand side (nullptr for NOT)
    std::unique_ptr<Expression> right; ///< Right-hand side expression
};

//===---------------------------------------------------------------------===//
// Properties
//===---------------------------------------------------------------------===//

/// @brief Represents a property assignment
///
/// Properties are key-value pairs that configure build settings.
/// Values can be single or multiple items.
///
/// Example:
/// ```css
/// type: executable;
/// sources: "main.cpp", "utils.cpp";
/// cxx_standard: 20;
/// ```
struct Property final : NodeBase
{
    std::string key;           ///< Property key (e.g., "type", "sources")
    std::vector<Value> values; ///< Property values (can be multiple)
};

//===---------------------------------------------------------------------===//
// Top-Level Declarations
//===---------------------------------------------------------------------===//

/// @brief Top-level statement variant
///
/// Represents any statement that can appear at file scope or within blocks.
/// This includes declarations, control flow, and properties.
using Statement = std::variant<
  // Top-level declarations
  ProjectDecl,
  WorkspaceDecl,
  TargetDecl,
  DependenciesDecl,
  OptionsDecl,
  GlobalDecl,
  MixinDecl,
  PresetDecl,
  FeaturesDecl,
  TestingDecl,
  InstallDecl,
  PackageDecl,
  ScriptsDecl,
  ToolchainDecl,
  RootDecl,

  // Visibility blocks (only in target bodies)
  VisibilityBlock,

  // Control flow
  IfStmt,
  ForStmt,
  LoopControlStmt,
  DiagnosticStmt,
  ImportStmt,
  ApplyStmt,

  // Properties (can appear in blocks)
  Property>;

/// @brief Represents a project declaration
///
/// Defines the project name and global project settings.
///
/// Example:
/// ```css
/// project myproject {
///     version: "1.0.0";
///     language: "C++";
/// }
/// ```
struct ProjectDecl final : NodeBase
{
    std::string name;                 ///< Project name
    std::vector<Property> properties; ///< Project properties
};

/// @brief Represents a workspace declaration
///
/// Defines workspace-wide settings and configurations.
///
/// Example:
/// ```css
/// workspace {
///     build_dir: "build";
/// }
/// ```
struct WorkspaceDecl final : NodeBase
{
    std::vector<Property> properties; ///< Workspace properties
};

/// @brief Represents a target declaration
///
/// Defines a build target (executable, library, ...) with its configuration.
///
/// Example:
/// ```css
/// target mylib {
///     type: static_library;
///     sources: glob("src/*.cpp");
/// }
/// ```
struct TargetDecl final : NodeBase
{
    std::string name;            ///< Target name
    std::vector<Statement> body; ///< Target body (properties, visibility blocks, etc.)
};

/// @brief Represents a dependencies declaration
///
/// Lists external dependencies required by the project.
///
/// Example:
/// ```css
/// dependencies {
///     fmt: "10.0.0";
///     spdlog: "1.12.0";
/// }
/// ```
struct DependenciesDecl final : NodeBase
{
    std::vector<Property> dependencies; ///< List of dependencies
};

/// @brief Represents an options declaration
///
/// Defines build options that can be toggled or configured.
///
/// Example:
/// ```css
/// options {
///     BUILD_TESTS: true;
///     ENABLE_LOGGING: false;
/// }
/// ```
struct OptionsDecl final : NodeBase
{
    std::vector<Property> options; ///< Build options
};

/// @brief Represents a global declaration
///
/// Defines global settings that apply to all targets.
///
/// Example:
/// ```css
/// global {
///     cxx_standard: 20;
///     warnings: "all";
/// }
/// ```
struct GlobalDecl final : NodeBase
{
    std::vector<Property> properties; ///< Global properties
};

/// @brief Represents a mixin declaration
///
/// Defines reusable property sets that can be applied to targets.
///
/// Example:
/// ```css
/// mixin common_flags {
///     warnings: "all";
///     optimization: "O2";
/// }
/// ```
struct MixinDecl final : NodeBase
{
    std::string name;                 ///< Mixin name
    std::vector<Property> properties; ///< Mixin properties
};

/// @brief Represents a preset declaration
///
/// Defines build presets with predefined configurations.
///
/// Example:
/// ```css
/// preset release {
///     optimization: "O3";
///     debug_info: false;
/// }
/// ```
struct PresetDecl final : NodeBase
{
    std::string name;                 ///< Preset name
    std::vector<Property> properties; ///< Preset properties
};

/// @brief Represents a features declaration
///
/// Defines optional features that can be enabled or disabled.
///
/// Example:
/// ```css
/// features {
///     networking: true;
///     graphics: false;
/// }
/// ```
struct FeaturesDecl final : NodeBase
{
    std::vector<Property> features; ///< Feature flags
};

/// @brief Represents a testing declaration
///
/// Configures testing framework and test settings.
///
/// Example:
/// ```css
/// testing {
///     framework: "catch2";
///     timeout: 30;
/// }
/// ```
struct TestingDecl final : NodeBase
{
    std::vector<Property> properties; ///< Testing properties
};

/// @brief Represents an install declaration
///
/// Specifies installation rules and destinations.
///
/// Example:
/// ```css
/// install {
///     destination: "/usr/local/bin";
///     permissions: "755";
/// }
/// ```
struct InstallDecl final : NodeBase
{
    std::vector<Property> properties; ///< Installation properties
};

/// @brief Represents a package declaration
///
/// Defines packaging configuration for distribution.
///
/// Example:
/// ```css
/// package {
///     format: "deb";
///     maintainer: "user@example.com";
/// }
/// ```
struct PackageDecl final : NodeBase
{
    std::vector<Property> properties; ///< Package properties
};

/// @brief Represents a scripts declaration
///
/// Defines custom build scripts and hooks.
///
/// Example:
/// ```css
/// scripts {
///     pre_build: "./setup.sh";
///     post_build: "./cleanup.sh";
/// }
/// ```
struct ScriptsDecl final : NodeBase
{
    std::vector<Property> scripts; ///< Build scripts
};

/// @brief Represents a toolchain declaration
///
/// Specifies compiler toolchain configuration.
///
/// Example:
/// ```css
/// toolchain gcc {
///     compiler: "g++";
///     version: "13";
/// }
/// ```
struct ToolchainDecl final : NodeBase
{
    std::string name;                 ///< Toolchain name
    std::vector<Property> properties; ///< Toolchain properties
};

/// @brief Represents a root declaration for CSS-like custom properties
///
/// Defines custom variables that can be referenced elsewhere.
///
/// Example:
/// ```css
/// :root {
///     --version: "1.0.0";
///     --author: "niekdomi";
/// }
/// ```
struct RootDecl final : NodeBase
{
    std::vector<Property> variables; ///< Custom property definitions
};

//===---------------------------------------------------------------------===//
// Visibility Blocks
//===---------------------------------------------------------------------===//

/// @brief Visibility modifier for target properties
///
/// Controls how properties are propagated to dependent targets.
enum class Visibility : std::uint8_t
{
    PUBLIC,   ///< Visible to this target and all dependents
    PRIVATE,  ///< Visible only to this target
    INTERFACE ///< Visible only to dependents
};

/// @brief Represents a visibility block within a target
///
/// Groups properties with a specific visibility modifier.
///
/// Example:
/// ```css
/// target mylib {
///     public {
///         include_dirs: "include";
///     }
///     private {
///         sources: glob("src/*.cpp");
///     }
/// }
/// ```
struct VisibilityBlock final : NodeBase
{
    Visibility visibility;            ///< Visibility level
    std::vector<Property> properties; ///< Properties with this visibility
};

//===---------------------------------------------------------------------===//
// Control Flow
//===---------------------------------------------------------------------===//

/// @brief Represents a conditional statement
///
/// Allows conditional compilation based on platform, options, or expressions.
///
/// Example:
/// ```css
/// @if platform(windows) {
///     sources: "win32.cpp";
/// } @else-if platform(linux) {
///     sources: "linux.cpp";
/// } @else {
///     sources: "generic.cpp";
/// }
/// ```
struct IfStmt final : NodeBase
{
    Expression condition;              ///< Condition to evaluate
    std::vector<Statement> then_block; ///< Statements if condition is true
    std::vector<Statement> else_block; ///< Statements if condition is false (empty if no else)
};

/// @brief Represents a for loop statement
///
/// Iterates over a collection or range.
///
/// Example:
/// ```css
/// @for file in glob("src/*.cpp") {
///     sources: file;
/// }
/// ```
struct ForStmt final : NodeBase
{
    std::string variable;        ///< Loop variable name
    Expression iterable;         ///< Collection to iterate (list, range, or function call)
    std::vector<Statement> body; ///< Loop body statements
};

/// @brief Loop control operation type
enum class LoopControl : std::uint8_t
{
    BREAK,   ///< Exit loop immediately
    CONTINUE ///< Skip to next iteration
};

/// @brief Represents a loop control statement
///
/// Breaks out of or continues a loop.
///
/// Example:
/// ```css
/// @for item in items {
///     @if condition { @break; }
/// }
/// ```
/// @note NOLINTNEXTLINE (suppresses uninitialized member warning)
struct LoopControlStmt final : NodeBase
{
    LoopControl control; ///< Control operation (break or continue)
};

/// @brief Diagnostic message severity level
enum class DiagnosticLevel : std::uint8_t
{
    ERROR,   ///< Stops build
    WARNING, ///< Continues build
    INFO     ///< Informational
};

/// @brief Represents a diagnostic message statement
///
/// Emits errors, warnings, or info messages during build configuration.
///
/// Example:
/// ```css
/// @if not option(BUILD_TESTS) {
///     @warning "Tests are disabled";
/// }
/// @error "Unsupported platform";
/// ```
struct DiagnosticStmt final : NodeBase
{
    DiagnosticLevel level; ///< Message severity level
    std::string message;   ///< Diagnostic message text
};

/// @brief Represents an import statement
///
/// Imports declarations from another Kumi file.
///
/// Example:
/// ```css
/// @import "common.kumi";
/// @import "../shared/config.kumi";
/// ```
struct ImportStmt final : NodeBase
{
    std::string path; ///< Path to file to import (relative or absolute)
};

/// @brief Represents an apply statement
///
/// Applies a profile or mixin with optional overrides.
///
/// Example:
/// ```css
/// @apply profile(release) {
///     optimization: "O3";
/// }
/// ```
struct ApplyStmt final : NodeBase
{
    FunctionCall profile;        ///< Profile function call
    std::vector<Statement> body; ///< Override statements
};

//===---------------------------------------------------------------------===//
// Root AST
//===---------------------------------------------------------------------===//

/// @brief Root node representing an entire Kumi build file
struct AST
{
    std::vector<Statement> statements; ///< All top-level statements in the file
};

} // namespace kumi
