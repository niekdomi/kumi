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

#include "lang/support/arena.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace kumi::lang {

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
///
/// @note There is no default constructor to allow aggregate initialization.
struct NodeBase
{
    std::uint32_t position{}; ///< Character offset in source file
};

static_assert(sizeof(NodeBase) == 4);

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
using Value = std::variant<std::string_view, ///< String literal or identifier: "hello" or myapp
                           std::uint32_t,    ///< Integer literal: 42, 0, 1000
                           bool              ///< Boolean literal: true, false
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
    std::uint32_t element_start{}; ///< Start of list elements
    std::uint32_t element_end{};   ///< End of list elements
};

static_assert(sizeof(List) == 12);

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

static_assert(sizeof(Range) == 12);

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
    std::string_view name;     ///< Function name (platform, glob, arch, etc.)
    std::uint32_t arg_start{}; ///< Start of positional arguments
    std::uint32_t arg_end{};   ///< End of positional arguments
};

static_assert(sizeof(FunctionCall) == 32);

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
                                  std::uint32_t // Parenthesized expressions: a and b
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

static_assert(sizeof(UnaryExpr) == 48);

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
    UnaryExpr left;                     ///< Left-hand side
    ComparisonOperator op;              ///< Comparison operator (if binary)
    std::optional<std::uint32_t> right; ///< Right-hand side (if binary)
};

static_assert(sizeof(ComparisonExpr) == 72);

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
    LogicalOperator op;            ///< Operator (all same: AND or OR)
    std::uint32_t operand_start{}; ///< Start of comparison operands (2+)
    std::uint32_t operand_end{};   ///< End of comparison operands (2+)
};

static_assert(sizeof(LogicalExpr) == 16);

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
    std::string_view key;        ///< Property name (type, sources, defines, etc.)
    std::uint32_t value_start{}; ///< Start of property values (one or more)
    std::uint32_t value_end{};   ///< End of property values (one or more)
};

static_assert(sizeof(Property) == 32);

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
using DependencyValue = std::variant<std::string_view, ///< Version string: "1.0.0", "^2.3.4"
                                     FunctionCall      ///< git(...), path(...), system
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
    bool is_optional{false};    ///< True if suffixed with `?`
    std::string_view name;      ///< Dependency name (package identifier)
    DependencyValue value;      ///< Version, git URL, path, or system
    std::uint32_t option_start; ///< Start of additional options (`tag`, `branch`, ...)
    std::uint32_t option_end;   ///< End of additional options
};

static_assert(sizeof(DependencySpec) == 72);

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
    std::string_view name;            ///< Option name
    Value default_value;              ///< Default value
    std::uint32_t constraint_start{}; ///< Start of constraints (min, max, choices, etc.)
    std::uint32_t constraint_end{};   ///< End of constraints (min, max, choices, etc.)
};

static_assert(sizeof(OptionSpec) == 56);

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
    std::string_view name;          ///< Project name
    std::uint32_t property_start{}; ///< Start of project configuration properties
    std::uint32_t property_end{};   ///< End of project configuration properties
};

static_assert(sizeof(ProjectDecl) == 32);

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
    std::uint32_t property_start{}; ///< Start of workspace configuration
    std::uint32_t property_end{};   ///< End of workspace configuration
};

static_assert(sizeof(WorkspaceDecl) == 12);

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
    std::string_view name;       ///< Target name
    std::uint32_t mixin_start{}; ///< Start of mixins to apply (via `with` keyword)
    std::uint32_t mixin_end{};   ///< End of mixins to apply
    std::uint32_t body_start;    ///< Start of target body (`properties`, `blocks`, `control flow`)
    std::uint32_t body_end{};    ///< End of target body
};

static_assert(sizeof(TargetDecl) == 40);

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
    std::uint32_t dep_start{}; ///< Start of list of dependency specifications
    std::uint32_t dep_end{};   ///< End of list of dependency specifications
};

static_assert(sizeof(DependenciesDecl) == 12);

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
    std::uint32_t option_start{}; ///< Start of build option specifications
    std::uint32_t option_end{};   ///< End of build option specifications
};

static_assert(sizeof(OptionsDecl) == 12);

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
    std::string_view name;      ///< Mixin name
    std::uint32_t body_start{}; ///< Start of mixin body (properties, visibility blocks)
    std::uint32_t body_end{};   ///< End of mixin body (properties, visibility blocks)
};

static_assert(sizeof(MixinDecl) == 32);

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
/// }
/// ```
struct ProfileDecl final : NodeBase
{
    std::string_view name;          ///< Profile name (debug, release, etc.)
    std::uint32_t mixin_start{};    ///< Start of mixins to apply
    std::uint32_t mixin_end{};      ///< End of mixins to apply
    std::uint32_t property_start{}; ///< Start of profile configuration
    std::uint32_t property_end{};   ///< End of profile configuration
};

static_assert(sizeof(ProfileDecl) == 40);

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
    std::uint32_t property_start{}; ///< Start of installation configuration
    std::uint32_t property_end{};   ///< End of installation configuration
};

static_assert(sizeof(InstallDecl) == 12);

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
    std::uint32_t property_start{}; ///< Start of package configuration
    std::uint32_t property_end{};   ///< End of package configuration
};

static_assert(sizeof(PackageDecl) == 12);

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
    std::uint32_t script_start{}; ///< Start of build script hooks
    std::uint32_t script_end{};   ///< End of build script hooks
};

static_assert(sizeof(ScriptsDecl) == 12);

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
    Visibility visibility{};        ///< Visibility level
    std::uint32_t property_start{}; ///< Start of properties with this visibility
    std::uint32_t property_end{};   ///< End of properties with this visibility
};

static_assert(sizeof(VisibilityBlock) == 16);

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
    Condition condition;      ///< Condition to evaluate
    std::uint32_t then_start; ///< Start of statements if condition is true
    std::uint32_t then_count; ///< Number of statements in then block
    std::uint32_t else_start; ///< Start of else/else-if block
    std::uint32_t else_count; ///< Number of statements in else block
};

static_assert(sizeof(IfStmt) == 104);

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
    std::string_view variable;  ///< Loop variable name
    Iterable iterable;          ///< Collection to iterate over
    std::uint32_t body_start{}; ///< Start of loop body statements
    std::uint32_t body_end{};   ///< End of loop body statements
};

static_assert(sizeof(ForStmt) == 72);

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

static_assert(sizeof(LoopControlStmt) == 8);

//===----------------------------------------------------------------------===//
// Diagnostic and Import Statements
//===----------------------------------------------------------------------===//

/// @brief Diagnostic message severity level
enum class DiagnosticLevel : std::uint8_t
{
    ERROR,       ///< Error: stops build
    WARNING,     ///< Warning: continues build
    INFO,        ///< Info: informational message
    DEBUG_LEVEL, ///< Debug: verbose debugging message (shown with --verbose)
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
    DiagnosticLevel level{};  ///< Message severity
    std::string_view message; ///< Diagnostic message text
};

static_assert(sizeof(DiagnosticStmt) == 24);

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
    std::string_view path; ///< Path to file to import (relative or absolute)
};

static_assert(sizeof(ImportStmt) == 24);

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
    // String storage
    Arena string_storage;

    // Side vectors (SOA)
    std::vector<Property> all_properties;
    std::vector<std::string_view> all_mixins;
    std::vector<Statement> all_statements;
    std::vector<DependencySpec> all_dependencies;
    std::vector<OptionSpec> all_options;

    // Main declarations
    std::vector<Statement> statements;
    std::string_view file_path;

    // Access helpers
    [[nodiscard]]
    auto get_properties(uint32_t start, uint32_t count) const -> std::span<const Property>
    {
        return std::span{all_properties}.subspan(start, count);
    }

    [[nodiscard]]
    auto get_mixins(uint32_t start, uint32_t count) const -> std::span<const std::string_view>
    {
        return std::span{all_mixins}.subspan(start, count);
    }

    [[nodiscard]]
    auto get_statements(uint32_t start, uint32_t count) const -> std::span<const Statement>
    {
        return std::span{all_statements}.subspan(start, count);
    }

    [[nodiscard]]
    auto get_dependencies(uint32_t start, uint32_t count) const -> std::span<const DependencySpec>
    {
        return std::span{all_dependencies}.subspan(start, count);
    }

    [[nodiscard]]
    auto get_options(uint32_t start, uint32_t count) const -> std::span<const OptionSpec>
    {
        return std::span{all_options}.subspan(start, count);
    }

    AST() = default;
    AST(AST&&) = default;
    AST(const AST&) = delete;

    auto operator=(AST&&) -> AST& = default;
    auto operator=(const AST&) -> AST& = delete;
};

} // namespace kumi::lang
