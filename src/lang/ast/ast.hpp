/// @file ast.hpp
/// @brief Abstract Syntax Tree node definitions for the Kumi build system
///
/// The hierarchy:
/// - Expressions: Values, operators, function calls
/// - Statements: Declarations, control flow, properties
/// - Declarations: Top-level constructs (`project`, `target`, ...)
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

struct ComparisonExpr;
struct DependenciesDecl;
struct DiagnosticStmt;
struct ForStmt;
struct IfStmt;
struct ImportStmt;
struct InstallDecl;
struct LogicalExpr;
struct LoopControlStmt;
struct MixinDecl;
struct OptionsDecl;
struct PackageDecl;
struct ProfileDecl;
struct ProjectDecl;
struct ScriptsDecl;
struct TargetDecl;
struct UnaryExpr;
struct VisibilityBlock;
struct WorkspaceDecl;

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
    std::uint32_t idx{}; ///< Character offset in source file
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
struct List final : public NodeBase
{
    std::uint32_t element_start_idx{}; ///< Index of the start of list elements
    std::uint32_t element_end_idx{};   ///< Index of the end of list elements
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
struct Range final : public NodeBase
{
    std::uint32_t start_idx{}; ///< Index of the start value
    std::uint32_t end_idx{};   ///< Index of the end value
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
struct FunctionCall final : public NodeBase
{
    std::uint32_t name_idx{};      ///< Index of function name (platform, glob, arch, ...)
    std::uint32_t arg_start_idx{}; ///< Index of the start of positional arguments
    std::uint32_t arg_end_idx{};   ///< Index of the end of positional arguments
};

static_assert(sizeof(FunctionCall) == 16);

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

/// @brief Types of operands in unary expressions
enum class OperandType : std::uint8_t
{
    VALUE,         ///< A simple value: identifier, boolean, number: `true`, `DEBUG_MODE`
    LOGICAL_EXPR,  ///< A parenthesized logical expression: `(a and b)`
    FUNCTION_CALL, ///< A function call: `platform(windows)`
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
struct UnaryOperand final
{
    OperandType type;
    std::uint32_t idx{}; //< Index of the operand
};

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
struct UnaryExpr final : public NodeBase
{
    bool is_negated{false}; ///< True if prefixed with 'not'
    UnaryOperand operand;   ///< The operand to potentially negate
};

static_assert(sizeof(UnaryExpr) == 16);

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
struct ComparisonExpr final : public NodeBase
{
    std::uint32_t left_idx{};               ///< Index of Left-hand side expression
    std::optional<ComparisonOperator> op;   ///< Comparison operator (if binary)
    std::optional<std::uint32_t> right_idx; ///< Index of the right-hand side expression (if binary)
};

static_assert(sizeof(ComparisonExpr) == 20);

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
struct LogicalExpr final : public NodeBase
{
    std::uint32_t operand_start_idx{}; ///< Index of the start of comparison operands
    LogicalOperator op;                ///< Operator: `and` or `or`
    std::uint32_t operand_end_idx{};   ///< Index of the end of comparison operands
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
struct Property final : public NodeBase
{
    std::uint32_t name_idx{};        ///< Index of the function name (type, sources, defines, ...)
    std::uint32_t value_start_idx{}; ///< Index of the start of property values (one or more)
    std::uint32_t value_end_idx{};   ///< Index of the end of property values (one or more)
};

static_assert(sizeof(Property) == 16);

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
struct DependencySpec final : public NodeBase
{
    std::uint32_t name_idx{};         ///< Index of the dependency name (package identifier)
    DependencyValue value;            ///< Version, git URL, path, or system
    std::uint32_t option_start_idx{}; ///< Index of the start of additional options such as `tag`
    std::uint32_t option_end_idx{};   ///< Index of the end of additional options
    bool is_optional{false};          ///< True if suffixed with `?`
};

static_assert(sizeof(DependencySpec) == 48);

//===----------------------------------------------------------------------===//
// Options
//===----------------------------------------------------------------------===//

/// @brief Represents a build option specification
///
/// Options are user-configurable build settings that can be overridden
/// via command line or configuration files. They can include constraints
/// like allowed values, min/max ranges, ...
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
struct OptionSpec final : public NodeBase
{
    std::uint32_t name_idx{};             ///< Index of the option name
    Value default_value;                  ///< Default value
    std::uint32_t constraint_start_idx{}; ///< index of the start of constraints such as `min`
    std::uint32_t constraint_end_idx{};   ///< index of the end of constraints such as `min`
};

static_assert(sizeof(OptionSpec) == 40);

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
struct ProjectDecl final : public NodeBase
{
    std::uint32_t name_idx{};           ///< Index of the project name
    std::uint32_t property_start_idx{}; ///< Index of the start of project configuration properties
    std::uint32_t property_end_idx{};   ///< Index of the end of project configuration properties
};

static_assert(sizeof(ProjectDecl) == 16);

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
struct WorkspaceDecl final : public NodeBase
{
    std::uint32_t property_start_idx{}; ///< Index of the start of workspace configuration
    std::uint32_t property_end_idx{};   ///< Index of the end of workspace configuration
};

static_assert(sizeof(WorkspaceDecl) == 12);

/// @brief Represents a target declaration
///
/// Targets are the primary build outputs (executables, libraries, tests, ...).
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
struct TargetDecl final : public NodeBase
{
    std::uint32_t name_idx{};        ///< Index of the target name
    std::uint32_t mixin_start_idx{}; ///< Index of the start of mixins to apply
    std::uint32_t mixin_end_idx{};   ///< Index of the end of mixins to apply
    std::uint32_t body_start_idx{};  ///< Index of the start of target body as `properties`
    std::uint32_t body_end_idx{};    ///< Index of the end of target body as `properties`
};

static_assert(sizeof(TargetDecl) == 24);

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
struct DependenciesDecl final : public NodeBase
{
    std::uint32_t dep_start_idx{}; ///< Index of the start of list of dependency specifications
    std::uint32_t dep_end_idx{};   ///< Index of the end of list of dependency specifications
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
struct OptionsDecl final : public NodeBase
{
    std::uint32_t option_start_idx{}; ///< Index of the start of build option specifications
    std::uint32_t option_end_idx{};   ///< Index of the end of build option specifications
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
struct MixinDecl final : public NodeBase
{
    std::uint32_t name_idx{};       ///< Index of the mixin name
    std::uint32_t body_start_idx{}; ///< Index of the start of mixin body
    std::uint32_t body_end_idx{};   ///< Index of the end of mixin body
};

static_assert(sizeof(MixinDecl) == 16);

/// @brief Represents a profile declaration
///
/// Profiles define build configurations (debug, release, ...) with
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
struct ProfileDecl final : public NodeBase
{
    std::uint32_t name_idx{};           ///< Index of the profile name (debug, release, ...)
    std::uint32_t mixin_start_idx{};    ///< Index of the start of mixins to apply
    std::uint32_t mixin_end_idx{};      ///< Index of the end of mixins to apply
    std::uint32_t property_start_idx{}; ///< Index of the start of profile configuration
    std::uint32_t property_end_idx{};   ///< Index of the end of profile configuration
};

static_assert(sizeof(ProfileDecl) == 24);

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
struct InstallDecl final : public NodeBase
{
    std::uint32_t property_start_idx{}; ///< Index of the start of installation configuration
    std::uint32_t property_end_idx{};   ///< Index of the end of installation configuration
};

static_assert(sizeof(InstallDecl) == 12);

/// @brief Represents a package declaration
///
/// Defines packaging configuration for distribution formats like
/// deb, rpm, msi, dmg, ...
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
struct PackageDecl final : public NodeBase
{
    std::uint32_t property_start_idx{}; ///< Index of the start of package configuration
    std::uint32_t property_end_idx{};   ///< Index of the end of package configuration
};

static_assert(sizeof(PackageDecl) == 12);

/// @brief Represents a scripts declaration
///
/// Defines custom build scripts and hooks that run at various
/// stages of the build process (pre-build, post-build, ...).
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
struct ScriptsDecl final : public NodeBase
{
    std::uint32_t script_start_idx{}; ///< Index of the start of build script hooks
    std::uint32_t script_end_idx{};   ///< Index of the end of build script hooks
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
struct VisibilityBlock final : public NodeBase
{
    Visibility visibility{};            ///< Visibility level
    std::uint32_t property_start_idx{}; ///< Index of the start of properties with this visibility
    std::uint32_t property_end_idx{};   ///< Index of the end of properties with this visibility
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
struct IfStmt final : public NodeBase
{
    Condition condition;          ///< Condition to evaluate
    std::uint32_t then_start_idx; ///< Index of start of then block statements
    std::uint32_t then_end_idx;   ///< Index of end of then block statements
    std::uint32_t else_start_idx; ///< Index of start of else block statements
    std::uint32_t else_end_idx;   ///< Index of end of else block statements
};

static_assert(sizeof(IfStmt) == 44);

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
struct ForStmt final : public NodeBase
{
    std::uint32_t variable_name_idx{}; ///< Index of the loop variable name
    Iterable iterable;                 ///< Collection to iterate over
    std::uint32_t body_start_idx{};    ///< Index of the start of loop body statements
    std::uint32_t body_end_idx{};      ///< Index of the end of loop body statements
};

static_assert(sizeof(ForStmt) == 36);

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
struct LoopControlStmt final : public NodeBase
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
struct DiagnosticStmt final : public NodeBase
{
    DiagnosticLevel level{};     ///< Message severity
    std::uint32_t message_idx{}; ///< Index of the diagnostics message text
};

static_assert(sizeof(DiagnosticStmt) == 12);

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
struct ImportStmt final : public NodeBase
{
    std::uint32_t path_idx{}; ///< Index of the import path string
};

static_assert(sizeof(ImportStmt) == 8);

//===----------------------------------------------------------------------===//
// Root AST
//===----------------------------------------------------------------------===//

/// @brief Root AST node representing a complete Kumi build file
///
/// The AST uses a Structure of Arrays (SoA) design pattern for memory
/// efficiency and cache locality. Instead of storing child nodes directly in
/// parent nodes, all nodes of the same type are stored in contiguous vectors
/// and reference them via start/end indices.
struct AST final
{
    Arena string_storage; ///<  Arena allocator for all string data

    AST() = default;
    ~AST() = default;
    AST(AST&&) = default;
    AST(const AST&) = delete;
    auto operator=(AST&&) -> AST& = default;
    auto operator=(const AST&) -> AST& = delete;

    //===----------------------------------------------------------------------===//
    // Side vectors (Structure of Arrays)
    //===----------------------------------------------------------------------===//

    std::vector<ComparisonExpr> all_comparison_exprs; ///< Storage for all ComparisonExpr nodes
    std::vector<DependencySpec> all_dependencies;     ///< Storage for all DependencySpec nodes
    std::vector<FunctionCall> all_function_calls;     ///< Storage for all FunctionCall nodes
    std::vector<LogicalExpr> all_logical_exprs;       ///< Storage for all LogicalExpr nodes
    std::vector<OptionSpec> all_options;              ///< Storage for all OptionSpec nodes
    std::vector<Property> all_properties;             ///< Storage for all Property nodes
    std::vector<Statement> all_statements;            ///< Storage for all Statement nodes
    std::vector<UnaryExpr> all_unary_exprs;           ///< Storage for all UnaryExpr nodes
    std::vector<UnaryOperand> all_unary_operands;     ///< Storage for all UnaryOperand nodes
    std::vector<Value> all_values; ///< Storage for all Value nodes (literals and identifiers)
    std::vector<std::string_view> all_strings; ///< Storage for all strings referenced by the AST

    /// @brief Top-level statements in the build file
    ///
    /// This is the main entry point for AST traversal. Contains all root-level
    /// declarations (`project`, `workspace`, `targets`, `dependencies`, ...) in
    /// parse order. This is used to iterate over the file's top-level structure.
    std::vector<Statement> statements;

    /// @brief Index of the source file path in all_strings
    ///
    /// References the path to the source file this AST represents. Used for
    /// error reporting and import resolution.
    std::string_view file_path;

    //===----------------------------------------------------------------------===//
    // Access helpers
    //===----------------------------------------------------------------------===//

    /// @brief Retrieves a span of dependency specifications
    /// @param start_idx Index of first dependency in all_dependencies
    /// @param end_idx Index one past last dependency (exclusive)
    /// @return Span view of dependencies in range [start_idx, end_idx)
    [[nodiscard]]
    auto get_dependencies(std::uint32_t start_idx, std::uint32_t end_idx) const noexcept
      -> std::span<const DependencySpec>
    {
        return std::span{all_dependencies}.subspan(start_idx, end_idx - start_idx);
    }

    /// @brief Retrieves a span of option specifications
    /// @param start_idx Index of first option in all_options
    /// @param end_idx Index one past last option (exclusive)
    /// @return Span view of options in range [start_idx, end_idx)
    [[nodiscard]]
    auto get_options(std::uint32_t start_idx, std::uint32_t end_idx) const noexcept
      -> std::span<const OptionSpec>
    {
        return std::span{all_options}.subspan(start_idx, end_idx - start_idx);
    }

    /// @brief Retrieves a span of properties
    /// @param start_idx Index of first property in all_properties
    /// @param end_idx Index one past last property (exclusive)
    /// @return Span view of properties in range [start_idx, end_idx)
    [[nodiscard]]
    auto get_properties(std::uint32_t start_idx, std::uint32_t end_idx) const noexcept
      -> std::span<const Property>
    {
        return std::span{all_properties}.subspan(start_idx, end_idx - start_idx);
    }

    /// @brief Retrieves a span of statements (for nested blocks)
    /// @param start_idx Index of first statement in all_statements
    /// @param end_idx Index one past last statement (exclusive)
    /// @return Span view of statements in range [start_idx, end_idx)
    [[nodiscard]]
    auto get_statements(std::uint32_t start_idx, std::uint32_t end_idx) const noexcept
      -> std::span<const Statement>
    {
        return std::span{all_statements}.subspan(start_idx, end_idx - start_idx);
    }

    /// @brief Retrieves a string by index
    /// @param idx Index into all_strings
    /// @return String view at the given index
    [[nodiscard]]
    auto get_string(std::uint32_t idx) const noexcept -> std::string_view
    {
        return all_strings[idx];
    }

    /// @brief Retrieves a value by index
    /// @param idx Index into all_values
    /// @return Value at the given index
    [[nodiscard]]
    auto get_value(std::uint32_t idx) const noexcept -> const Value&
    {
        return all_values[idx];
    }
};

} // namespace kumi::lang
