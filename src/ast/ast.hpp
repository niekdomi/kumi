/// @file ast.hpp
/// @brief Abstract Syntax Tree node definitions for the Kumi build system
///
/// The hierarchy:
/// - Expressions: Values, operators, function calls
/// - Statements: Declarations, control flow, properties
/// - Declarations: Top-level constructs (project, target, etc.)
///
/// @see Parser for AST construction from tokens
/// @see Lexer for tokenization of source files

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace kumi {

//===----------------------------------------------------------------------===//
// Forward Declarations
//===----------------------------------------------------------------------===//

struct DependenciesDecl;
struct DiagnosticStmt;
struct ForStmt;
struct IfStmt;
struct ImportStmt;
struct InstallDecl;
struct LoopControlStmt;
struct MixinDecl;
struct OptionsDecl;
struct PackageDecl;
struct ProfileDecl;
struct ProjectDecl;
struct ScriptsDecl;
struct TargetDecl;
struct VisibilityBlock;
struct UnaryExpr;
struct WorkspaceDecl;
struct ComparisonExpr;
struct LogicalExpr;

//===----------------------------------------------------------------------===//
// Base Node
//===----------------------------------------------------------------------===//

/// @brief Base class for all AST nodes providing source location tracking
///
/// Every AST node includes position information for error reporting and
/// debugging. The position is a byte offset into the source file.
struct NodeBase
{
    std::uint32_t position{}; ///< Character offset in source file
};

//===----------------------------------------------------------------------===//
// Primitive Values
//===----------------------------------------------------------------------===//

/// @brief Represents a literal value (string, number, boolean, or identifier)
///
/// Values are the atomic units in the AST. They appear in property assignments,
/// function arguments, and expressions.
///
/// Examples:
/// ```css
/// name: "myapp";           // String value
/// version: 42;             // Integer value
/// enabled: true;           // Boolean value
/// type: executable;        // Identifier value
/// ```
using Value = std::variant<std::string,   ///< String literal or identifier: "hello" or myapp
                           std::uint32_t, ///< Integer literal: 42, 0, 1000
                           bool           ///< Boolean literal: true, false
                           >;

//===----------------------------------------------------------------------===//
// Expressions
//===----------------------------------------------------------------------===//

/// @brief Represents a list of values
///
/// Lists are used for property values and loop iteration.
///
/// Grammar:
/// ```ebnf
/// List = "[" Value { "," Value } "]" ;
/// ```
///
/// Examples:
/// ```css
/// sources: ["main.cpp", "utils.cpp", "helper.cpp"];
/// @for module in [core, renderer, audio] { ... }
/// ```
struct List final : NodeBase
{
    std::vector<Value> elements; ///< List elements
};

/// @brief Represents a numeric range for iteration
///
/// Ranges are inclusive of the start and exclusive of the end value,
/// following common programming convention.
///
/// Grammar:
/// ```ebnf
/// Range = Number ".." Number ;
/// ```
///
/// Examples:
/// ```css
/// @for i in 0..10 { ... }      // 0, 1, 2, ..., 9
/// @for worker in 1..8 { ... }  // 1, 2, 3, ..., 7
/// ```
struct Range final : NodeBase
{
    std::uint32_t start{}; ///< Start value (inclusive)
    std::uint32_t end{};   ///< End value (exclusive)
};

/// @brief Represents a function call expression
///
/// Function calls query build-time information like platform, architecture,
/// configuration, or perform operations like file globbing.
///
/// Grammar:
/// ```ebnf
/// FunctionCall = Identifier "(" [ ArgumentList ] ")" [ FunctionOptions ] ;
/// ArgumentList = Value { "," Value } ;
/// FunctionOptions = "{" { PropertyAssignment } "}" ;
/// ```
///
/// Examples:
/// ```css
/// @if platform(windows) { ... }
/// sources: glob("src/**/*.cpp");
/// @if arch(x86_64, arm64) { ... }
/// files: find("resources", "*.png");
/// ```
struct FunctionCall final : NodeBase
{
    std::string name;             ///< Function name (platform, glob, arch, etc.)
    std::vector<Value> arguments; ///< Positional arguments
};

/// @brief Primary expression (leaf nodes in expression tree)
///
/// Primary expressions are the most basic expressions that cannot be
/// further decomposed. They include literals, function calls, and
/// parenthesized expressions.
///
/// Grammar:
/// ```ebnf
/// PrimaryExpr = FunctionCall | Identifier | Boolean | "(" LogicalExpr ")" ;
/// ```
using PrimaryExpr = std::variant<FunctionCall,
                                 Value // Identifier or Boolean
                                 >;

/// @brief Logical operators for boolean expressions
enum class LogicalOperator : std::uint8_t
{
    AND, ///< Logical AND: `and`
    OR,  ///< Logical OR: `or`
};

/// @brief Comparison operators for relational expressions
enum class ComparisonOperator : std::uint8_t
{
    EQUAL,         ///< Equal: `==`
    NOT_EQUAL,     ///< Not equal: `!=`
    LESS,          ///< Less than: `<`
    LESS_EQUAL,    ///< Less than or equal: `<=`
    GREATER,       ///< Greater than: `>`
    GREATER_EQUAL, ///< Greater than or equal: `>=`
};

/// @brief Unary expression operand (primary expression or parenthesized logical expression)
///
/// This allows for both simple expressions like `platform(windows)` and
/// complex parenthesized expressions like `(a and b)`.
///
/// Grammar:
/// ```ebnf
/// UnaryOperand = FunctionCall | Identifier | Boolean | "(" LogicalExpr ")" ;
/// ```
using UnaryOperand = std::variant<FunctionCall,
                                  Value,
                                  std::unique_ptr<LogicalExpr> // Parenthesized expressions: a and b
                                  >;

/// @brief Represents a unary expression with optional NOT operator
///
/// Unary expressions handle logical negation.
///
/// Grammar:
/// ```ebnf
/// UnaryExpr = [ "not" ] PrimaryExpr ;
/// ```
///
/// Examples:
/// ```css
/// @if not platform(windows) { ... }
/// @if not feature(networking) { ... }
/// ```
struct UnaryExpr final : NodeBase
{
    bool is_negated{false}; ///< True if prefixed with 'not'
    UnaryOperand operand;   ///< The operand to potentially negate
};

/// @brief Represents a comparison expression
///
/// Comparison expressions evaluate relational operations between two values.
///
/// Grammar:
/// ```ebnf
/// ComparisonExpr = UnaryExpr [ ComparisonOp UnaryExpr ] ;
/// ComparisonOp = "==" | "!=" | "<" | ">" | "<=" | ">=" ;
/// ```
///
/// Examples:
/// ```css
/// @if option(MAX_THREADS) > 8 { ... }
/// @if version == 2 { ... }
/// @if arch(x86_64) { ... }  // Unary expression without comparison
/// ```
struct ComparisonExpr final : NodeBase
{
    UnaryExpr left;                       ///< Left-hand side
    std::optional<ComparisonOperator> op; ///< Comparison operator (if binary)
    std::optional<UnaryExpr> right;       ///< Right-hand side (if binary)
};

/// @brief Represents a logical expression (AND/OR of comparisons)
///
/// Logical expressions combine comparison expressions with AND/OR operators.
/// All operators in one expression must be the same (either all AND or all OR).
/// Mixed precedence requires parentheses.
///
/// Grammar:
/// ```ebnf
/// LogicalExpr = ComparisonExpr { ( "and" | "or" ) ComparisonExpr } ;
/// ```
///
/// Examples:
/// ```css
/// @if platform(windows) and arch(x86_64) { ... }
/// @if config(debug) or option(FORCE_LOGGING) { ... }
/// @if a and b and c { ... }
/// ```
struct LogicalExpr final : NodeBase
{
    LogicalOperator op;                   ///< Operator (all same: AND or OR)
    std::vector<ComparisonExpr> operands; ///< Comparison operands (2+)
};

/// @brief Top-level condition type used in if statements
///
/// A condition can be a logical expression, comparison, or simple unary expression.
///
/// Grammar:
/// ```ebnf
/// Condition = LogicalExpr ;
/// LogicalExpr = ComparisonExpr { ( "and" | "or" ) ComparisonExpr } ;
/// ```
using Condition = std::variant<LogicalExpr,    ///< a and b, x or y or z
                               ComparisonExpr, ///< a > 5, platform(windows)
                               UnaryExpr       ///< not feature(x), platform(linux)
                               >;

/// @brief Iterable expression for for-loops
///
/// Grammar:
/// ```ebnf
/// Iterable = List | Range | FunctionCall ;
/// ```
///
/// Examples:
/// ```css
/// @for x in [a, b, c] { ... }       // List
/// @for i in 0..10 { ... }           // Range
/// @for file in glob("*.cpp") { ... } // FunctionCall
/// ```
using Iterable = std::variant<List,        ///< Explicit list of values
                              Range,       ///< Numeric range
                              FunctionCall ///< Function returning iterable (e.g., glob)
                              >;

//===----------------------------------------------------------------------===//
// Properties
//===----------------------------------------------------------------------===//

/// @brief Represents a property assignment (key-value pair)
///
/// Properties configure build settings. They can have single or multiple values.
/// Semantic analysis validates whether multiple values are allowed and if
/// the value types are correct for each property.
///
/// Grammar:
/// ```ebnf
/// PropertyAssignment = Identifier ":" ValueList ";" ;
/// ValueList = Value { "," Value } ;
/// ```
///
/// Examples:
/// ```css
/// type: executable;                          // Single value
/// sources: "main.cpp", "utils.cpp";          // Multiple values
/// cxx_standard: 20;                          // Integer value
/// warnings: "all", "extra", "pedantic";      // Multiple string values
/// ```
struct Property final : NodeBase
{
    std::string key;           ///< Property name (type, sources, defines, etc.)
    std::vector<Value> values; ///< Property values (one or more)
};

//===----------------------------------------------------------------------===//
// Dependencies
//===----------------------------------------------------------------------===//

/// @brief Dependency value type
///
/// Dependencies can reference version strings, system packages,
/// git repositories, or local paths.
///
/// Grammar:
/// ```ebnf
/// DependencyValue = String | Identifier | FunctionCall ;
/// ```
using DependencyValue = std::variant<std::string, ///< Version string: "1.0.0", "^2.3.4"
                                     FunctionCall ///< git(...), path(...), system
                                     >;

/// @brief Represents a single dependency specification
///
/// Dependencies declare external libraries or packages required by the project.
/// They can be optional and include additional configuration options.
///
/// Grammar:
/// ```ebnf
/// DependencySpec = Identifier [ "?" ] ":" DependencyValue [ DependencyOptions ] ";" ;
/// DependencyOptions = "{" { PropertyAssignment } "}" ;
/// ```
///
/// Examples:
/// ```css
/// fmt: "10.2.1";    // Simple version
/// opengl?: system; // Optional system dependency
/// imgui: git("https://github.com/...") {
///     tag: "v1.90";
/// };
/// ```
struct DependencySpec final : NodeBase
{
    bool is_optional{false};       ///< True if suffixed with `?`
    std::string name;              ///< Dependency name (package identifier)
    DependencyValue value;         ///< Version, git URL, path, or system
    std::vector<Property> options; ///< Additional options (tag, branch, etc.)
};

//===----------------------------------------------------------------------===//
// Options
//===----------------------------------------------------------------------===//

/// @brief Represents a build option specification
///
/// Options are user-configurable build settings that can be overridden
/// via command line or configuration files. They can include constraints
/// like allowed values, min/max ranges, etc.
///
/// Grammar:
/// ```ebnf
/// OptionSpec = Identifier ":" Value [ OptionConstraints ] ";" ;
/// OptionConstraints = "{" { PropertyAssignment } "}" ;
/// ```
///
/// Examples:
/// ```css
/// BUILD_TESTS: true;
/// MAX_THREADS: 8 {
///     min: 1;
///     max: 128;
/// };
/// LOG_LEVEL: "info" {
///     choices: "debug", "info", "warning", "error";
/// };
/// ```
struct OptionSpec final : NodeBase
{
    std::string name;                  ///< Option name
    Value default_value;               ///< Default value
    std::vector<Property> constraints; ///< Constraints (min, max, choices, etc.)
};

//===----------------------------------------------------------------------===//
// Top-Level Declarations
//===----------------------------------------------------------------------===//

// Forward declare Statement for recursive definition
using Statement = std::variant<ProjectDecl,
                               WorkspaceDecl,
                               TargetDecl,
                               DependenciesDecl,
                               OptionsDecl,
                               MixinDecl,
                               ProfileDecl,
                               InstallDecl,
                               PackageDecl,
                               ScriptsDecl,
                               VisibilityBlock,
                               IfStmt,
                               ForStmt,
                               LoopControlStmt,
                               DiagnosticStmt,
                               ImportStmt,
                               Property>;

/// @brief Represents a project declaration
///
/// The project declaration defines the project name and global settings.
/// There should be exactly one project per build file.
///
/// Grammar:
/// ```ebnf
/// ProjectDecl = "project" Identifier "{" { PropertyAssignment } "}" ;
/// ```
///
/// Example:
/// ```css
/// project myapp {
///     version: "1.0.0";
///     cpp: 23;
///     license: "MIT";
/// }
/// ```
struct ProjectDecl final : NodeBase
{
    std::string name;                 ///< Project name
    std::vector<Property> properties; ///< Project configuration properties
};

/// @brief Represents a workspace declaration
///
/// The workspace declaration configures workspace-wide settings like
/// build directory, output paths, and global compiler flags.
///
/// Grammar:
/// ```ebnf
/// WorkspaceDecl = "workspace" "{" { PropertyAssignment } "}" ;
/// ```
///
/// Example:
/// ```css
/// workspace {
///     build-dir: "build";
///     output-dir: "dist";
///     warnings-as-errors: true;
/// }
/// ```
struct WorkspaceDecl final : NodeBase
{
    std::vector<Property> properties; ///< Workspace configuration
};

/// @brief Represents a target declaration
///
/// Targets are the primary build outputs (executables, libraries, tests, etc.).
/// They can compose mixins for shared configuration and contain properties,
/// visibility blocks, and conditional/loop statements.
///
/// Grammar:
/// ```ebnf
/// TargetDecl = "target" Identifier [ "with" MixinList ] "{" { TargetContent } "}" ;
/// MixinList = Identifier { "," Identifier } ;
/// TargetContent = PropertyAssignment | VisibilityBlock | Statement ;
/// ```
///
/// Examples:
/// ```css
/// target myapp {
///     type: executable;
///     sources: glob("src/*.cpp");
/// }
///
/// target mylib with common-flags, strict-warnings {
///     type: static-library;
///     public {
///         include-dirs: "include";
///     }
///     private {
///         sources: glob("src/*.cpp");
///     }
/// }
/// ```
struct TargetDecl final : NodeBase
{
    std::string name;                ///< Target name
    std::vector<std::string> mixins; ///< Mixins to apply (via `with` keyword)
    std::vector<Statement> body;     ///< Target body (properties, blocks, control flow)
};

/// @brief Represents a dependencies declaration
///
/// Declares all external dependencies required by the project.
///
/// Grammar:
/// ```ebnf
/// DependenciesDecl = "dependencies" "{" { DependencySpec } "}" ;
/// ```
///
/// Example:
/// ```css
/// dependencies {
///     fmt: "10.2.1";
///     spdlog: "1.12.0";
///     opengl?: system;
///     imgui: git("https://github.com/ocornut/imgui") {
///         tag: "v1.90";
///     };
/// }
/// ```
struct DependenciesDecl final : NodeBase
{
    std::vector<DependencySpec> dependencies; ///< List of dependency specifications
};

/// @brief Represents an options declaration
///
/// Defines user-configurable build options with default values and constraints.
///
/// Grammar:
/// ```ebnf
/// OptionsDecl = "options" "{" { OptionSpec } "}" ;
/// ```
///
/// Example:
/// ```css
/// options {
///     BUILD_TESTS: true;
///     MAX_THREADS: 8 {
///         min: 1;
///         max: 128;
///     };
///     LOG_LEVEL: "info" {
///         choices: "debug", "info", "warning", "error";
///     };
/// }
/// ```
struct OptionsDecl final : NodeBase
{
    std::vector<OptionSpec> options; ///< Build option specifications
};

/// @brief Represents a mixin declaration
///
/// Mixins are reusable property sets that can be composed into targets
/// and profiles. They help avoid repetition and enforce consistency.
///
/// Grammar:
/// ```ebnf
/// MixinDecl = "mixin" Identifier "{" { PropertyAssignment | VisibilityBlock } "}" ;
/// ```
///
/// Example:
/// ```css
/// mixin strict-warnings {
///     warnings: "all", "extra", "pedantic";
///     warnings-as-errors: true;
/// }
///
/// mixin embedded-target {
///     optimization: size;
///     public {
///         defines: EMBEDDED_TARGET;
///     }
/// }
/// ```
struct MixinDecl final : NodeBase
{
    std::string name;            ///< Mixin name
    std::vector<Statement> body; ///< Mixin body (properties, visibility blocks)
};

/// @brief Represents a profile declaration
///
/// Profiles define build configurations (debug, release, etc.) with
/// specific compiler and linker settings. They can compose mixins.
///
/// Grammar:
/// ```ebnf
/// ProfileDecl = "profile" Identifier [ "with" MixinList ] "{" { PropertyAssignment } "}" ;
/// ```
///
/// Example:
/// ```css
/// profile debug {
///     optimize: none;
///     debug-info: full;
///     defines: DEBUG_MODE;
/// }
///
/// profile release with lto-flags {
///     optimize: aggressive;
///     debug-info: none;
///     defines: NDEBUG;
/// }
/// ```
struct ProfileDecl final : NodeBase
{
    std::string name;                 ///< Profile name (debug, release, etc.)
    std::vector<std::string> mixins;  ///< Mixins to apply
    std::vector<Property> properties; ///< Profile configuration
};

/// @brief Represents an install declaration
///
/// Specifies installation rules, destinations, and file permissions.
/// Controls how build artifacts are deployed to target systems.
///
/// Grammar:
/// ```ebnf
/// InstallDecl = "install" "{" { PropertyAssignment } "}" ;
/// ```
///
/// Example:
/// ```css
/// install {
///     destination: "/usr/local/bin";
///     permissions: "755";
///     targets: myapp, mylib;
/// }
/// ```
struct InstallDecl final : NodeBase
{
    std::vector<Property> properties; ///< Installation configuration
};

/// @brief Represents a package declaration
///
/// Defines packaging configuration for distribution formats like
/// deb, rpm, msi, dmg, etc.
///
/// Grammar:
/// ```ebnf
/// PackageDecl = "package" "{" { PropertyAssignment } "}" ;
/// ```
///
/// Example:
/// ```css
/// package {
///     format: "deb", "rpm";
///     maintainer: "user@example.com";
///     description: "My awesome application";
///     license: "MIT";
/// }
/// ```
struct PackageDecl final : NodeBase
{
    std::vector<Property> properties; ///< Package configuration
};

/// @brief Represents a scripts declaration
///
/// Defines custom build scripts and hooks that run at various
/// stages of the build process (pre-build, post-build, etc.).
///
/// Grammar:
/// ```ebnf
/// ScriptsDecl = "scripts" "{" { PropertyAssignment } "}" ;
/// ```
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
    std::vector<Property> scripts; ///< Build script hooks
};

//===----------------------------------------------------------------------===//
// Visibility Blocks
//===----------------------------------------------------------------------===//

/// @brief Visibility modifier for target properties
///
/// Controls how properties propagate to dependent targets, following
/// CMake's model of transitive dependencies.
enum class Visibility : std::uint8_t
{
    PUBLIC,    ///< Visible to this target and all consumers
    PRIVATE,   ///< Visible only to this target
    INTERFACE, ///< Visible only to consumers (not this target)
};

/// @brief Represents a visibility block within a target or mixin
///
/// Visibility blocks group properties with a specific propagation level.
/// This is essential for proper dependency management in modular builds.
///
/// Grammar:
/// ```ebnf
/// VisibilityBlock = ( "public" | "private" | "interface" ) "{" { PropertyAssignment } "}" ;
/// ```
///
/// Example:
/// ```css
/// target mylib {
///     public {
///         include-dirs: "include";
///         defines: USE_MYLIB;
///     }
///     private {
///         sources: glob("src/*.cpp");
///         defines: MYLIB_INTERNAL;
///     }
///     interface {
///         link-libraries: pthread;
///     }
/// }
/// ```
struct VisibilityBlock final : NodeBase
{
    Visibility visibility;            ///< Visibility level
    std::vector<Property> properties; ///< Properties with this visibility
};

//===----------------------------------------------------------------------===//
// Control Flow Statements
//===----------------------------------------------------------------------===//

/// @brief Represents a conditional statement (if/else-if/else)
///
/// Conditionals enable platform-specific and configuration-dependent builds.
/// The else_branch can contain either more statements (for else block) or
/// another IfStmt (for else-if chains).
///
/// Grammar:
/// ```ebnf
/// ConditionalBlock = IfBlock [ ElseBlock ] ;
/// IfBlock = "@if" Condition "{" { Statement } "}" ;
/// ElseBlock = "@else" ( IfBlock | "{" { Statement } "}" ) ;
/// ```
///
/// Examples:
/// ```css
/// @if platform(windows) {
///     sources: "win32.cpp";
/// } @else-if platform(macos) {
///     sources: "macos.cpp";
/// } @else {
///     sources: "linux.cpp";
/// }
/// ```
struct IfStmt final : NodeBase
{
    Condition condition;               ///< Condition to evaluate
    std::vector<Statement> then_block; ///< Statements if condition is true
    std::vector<Statement> else_block; ///< Statements for else/else-if (may contain another IfStmt)
};

/// @brief Represents a for-loop statement
///
/// For-loops iterate over collections, ranges, or file globs, generating
/// targets or properties dynamically.
///
/// Grammar:
/// ```ebnf
/// ForLoop = "@for" Identifier "in" Iterable "{" { Statement } "}" ;
/// Iterable = List | Range | FunctionCall ;
/// ```
///
/// Examples:
/// ```css
/// @for module in [core, renderer, audio] {
///     target ${module}-lib {
///         type: static-library;
///         sources: "modules/${module}/**/*.cpp";
///     }
/// }
///
/// @for i in 0..8 {
///     target worker-${i} {
///         defines: WORKER_ID=${i};
///     }
/// }
///
/// @for file in glob("plugins/*.cpp") {
///     target plugin-${file.stem} {
///         sources: ${file};
///     }
/// }
/// ```
struct ForStmt final : NodeBase
{
    std::string variable;        ///< Loop variable name
    Iterable iterable;           ///< Collection to iterate over
    std::vector<Statement> body; ///< Loop body statements
};

/// @brief Loop control operation type
enum class LoopControl : std::uint8_t
{
    BREAK,    ///< Exit loop immediately: `@break;`
    CONTINUE, ///< Skip to next iteration: `@continue;`
};

/// @brief Represents a loop control statement
///
/// Loop control statements alter loop execution flow.
/// They are only valid inside for-loops.
///
/// Grammar:
/// ```ebnf
/// LoopControl = ( "@break" | "@continue" ) ";" ;
/// ```
///
/// Example:
/// ```css
/// @for file in glob("src/**/*.cpp") {
///     @if file.contains("deprecated") {
///         @continue;
///     }
///     sources: ${file};
/// }
/// ```
struct LoopControlStmt final : NodeBase
{
    LoopControl control{LoopControl::BREAK}; ///< Break or continue
};

//===----------------------------------------------------------------------===//
// Diagnostic and Import Statements
//===----------------------------------------------------------------------===//

/// @brief Diagnostic message severity level
enum DiagnosticLevel : std::uint8_t
{
    ERROR,   ///< Error: stops build
    WARNING, ///< Warning: continues build
    INFO,    ///< Info: informational message
    DEBUG,   ///< Debug: verbose debugging message (shown with --verbose)
};

/// @brief Represents a diagnostic message statement
///
/// Diagnostics emit errors, warnings, or info messages during configuration.
/// Useful for validation and debugging build logic.
///
/// Grammar:
/// ```ebnf
/// DiagnosticStmt = ( "@error" | "@warning" | "@info" | "@debug" ) String ";" ;
/// ```
///
/// Examples:
/// ```css
/// @if not option(BUILD_TESTS) {
///     @warning "Tests are disabled";
/// }
///
/// @if platform(unknown) {
///     @error "Unsupported platform";
/// }
///
/// @info "Configuring with ${project.name} v${project.version}";
/// ```
struct DiagnosticStmt final : NodeBase
{
    DiagnosticLevel level; ///< Message severity
    std::string message;   ///< Diagnostic message text
};

/// @brief Represents an import statement
///
/// Imports declarations from another Kumi file, enabling modular
/// build configurations.
///
/// Grammar:
/// ```ebnf
/// ImportDecl = "@import" String ";" ;
/// ```
///
/// Examples:
/// ```css
/// @import "common/mixins.kumi";
/// @import "../shared/options.kumi";
/// ```
struct ImportStmt final : NodeBase
{
    std::string path; ///< Path to file to import (relative or absolute)
};

//===----------------------------------------------------------------------===//
// Root AST
//===----------------------------------------------------------------------===//

/// @brief Root AST node representing a complete Kumi build file
///
/// The AST root contains all top-level statements parsed from the file.
/// A valid Kumi file should have one project declaration and any number
/// of other declarations and statements.
struct AST final
{
    std::vector<Statement> statements; ///< All top-level statements
    std::string file_path;             ///< Source file path (for diagnostics)
};

} // namespace kumi
