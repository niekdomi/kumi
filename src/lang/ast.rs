use std::mem::size_of;

// Abstract Syntax Tree node definitions for the Kumi build system
//
// The hierarchy:
// - Expressions: Values, operators, function calls
// - Statements: Declarations, control flow, properties
// - Declarations: Top-level constructs (`project`, `target`, ...)
//
// @see Parser for AST construction from tokens
// @see Lexer for tokenization of source files

//===----------------------------------------------------------------------===//
// Base Node
//===----------------------------------------------------------------------===//

/// Base class for all AST nodes providing source location tracking
///
/// Every AST node includes a source range (start + end byte offsets) for
/// error reporting, diagnostics, and tooling (LSP, formatter). The range
/// covers the full syntactic extent of the node, from the first token to
/// the last (inclusive of closing braces, semicolons, etc.).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct NodeBase {
    /// Index of the start byte offset in source file
    pub start_idx: u32,
    /// Index of the end byte offset in source file
    pub end_idx: u32,
}
const _: () = assert!(size_of::<NodeBase>() == 8);

impl NodeBase {
    pub fn new(start_idx: u32, end_idx: u32) -> Self {
        Self { start_idx, end_idx }
    }
}

//===----------------------------------------------------------------------===//
// Primitive Values
//===----------------------------------------------------------------------===//

/// Represents a literal value (string, number, boolean, or identifier)
///
/// Values are the atomic units in the AST. They appear in property assignments,
/// function arguments, and expressions.
///
/// Examples:
/// ```kumi
/// name: "myapp";           // String value
/// version: 42;             // Integer value
/// enabled: true;           // Boolean value
/// type: executable;        // Identifier value
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Value<'a> {
    /// String literal: "hello", "path/to/file"
    String(&'a str),
    /// Identifier: myapp, foo_bar
    Identifier(&'a str),
    /// Integer literal: 42, 0, 1000
    Integer(u32),
    /// Boolean literal: true, false
    Boolean(bool),
}
const _: () = assert!(size_of::<Value>() == 24);

//===----------------------------------------------------------------------===//
// Expressions
//===----------------------------------------------------------------------===//

/// Represents a list of values
///
/// Lists are used for property values and loop iteration.
///
/// Grammar:
/// ```ebnf
/// List = "[" Value { "," Value } "]" ;
/// ```
///
/// Examples:
/// ```kumi
/// sources: ["main.cpp", "utils.cpp", "helper.cpp"];
/// @for module in [core, renderer, audio] { ... }
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct List {
    pub base: NodeBase,
    /// Index of the start of list elements
    pub element_start_idx: u32,
    /// Index of the end of list elements
    pub element_end_idx: u32,
}
const _: () = assert!(size_of::<List>() == 16);

/// Represents a numeric range for iteration
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
/// ```kumi
/// @for i in 0..10 { ... }      // 0, 1, 2, ..., 9
/// @for worker in 1..8 { ... }  // 1, 2, 3, ..., 7
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Range {
    pub base: NodeBase,
    /// Index of the start value
    pub start_idx: u32,
    /// Index of the end value
    pub end_idx: u32,
}
const _: () = assert!(size_of::<Range>() == 16);

/// Represents a function call expression
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
/// ```kumi
/// @if platform(windows) { ... }
/// sources: glob("src/**/*.cpp");
/// @if arch(x86_64, arm64) { ... }
/// files: find("resources", "*.png");
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct FunctionCall {
    pub base: NodeBase,
    /// Index of function name (platform, glob, arch, ...)
    pub name_idx: u32,
    /// Index of the start of positional arguments
    pub arg_start_idx: u32,
    /// Index of the end of positional arguments
    pub arg_end_idx: u32,
}
const _: () = assert!(size_of::<FunctionCall>() == 20);

/// Primary expression (leaf nodes in expression tree)
///
/// Primary expressions are the most basic expressions that cannot be
/// further decomposed. They include literals, function calls, and
/// parenthesized expressions.
///
/// Grammar:
/// ```ebnf
/// PrimaryExpr = FunctionCall | Identifier | Boolean | "(" LogicalExpr ")" ;
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum PrimaryExpr<'a> {
    FunctionCall(FunctionCall),
    /// Identifier or Boolean
    Value(Value<'a>),
}
const _: () = assert!(size_of::<PrimaryExpr>() == 24);

/// Logical operators for boolean expressions
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LogicalOperator {
    /// Logical AND: `and`
    And,
    /// Logical OR: `or`
    Or,
}

/// Comparison operators for relational expressions
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ComparisonOperator {
    /// Equal: `==`
    Equal,
    /// Not equal: `!=`
    NotEqual,
    /// Less than: `<`
    Less,
    /// Less than or equal: `<=`
    LessEqual,
    /// Greater than: `>`
    Greater,
    /// Greater than or equal: `>=`
    GreaterEqual,
}

/// Types of operands in unary expressions
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum OperandType {
    /// A simple value: identifier, boolean, number: `true`, `DEBUG_MODE`
    Value,
    /// A parenthesized logical expression: `(a and b)`
    LogicalExpr,
    /// A function call: `platform(windows)`
    FunctionCall,
}

/// Unary expression operand (primary expression or parenthesized logical expression)
///
/// This allows for both simple expressions like `platform(windows)` and
/// complex parenthesized expressions like `(a and b)`.
///
/// Grammar:
/// ```ebnf
/// UnaryOperand = FunctionCall | Identifier | Boolean | "(" LogicalExpr ")" ;
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct UnaryOperand {
    pub kind: OperandType,
    /// Index of the operand
    pub idx: u32,
}
const _: () = assert!(size_of::<UnaryOperand>() == 8);

/// Represents a unary expression with optional NOT operator
///
/// Unary expressions handle logical negation.
///
/// Grammar:
/// ```ebnf
/// UnaryExpr = [ "not" ] PrimaryExpr ;
/// ```
///
/// Examples:
/// ```kumi
/// @if not platform(windows) { ... }
/// @if not feature(networking) { ... }
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct UnaryExpr {
    pub base: NodeBase,
    /// True if prefixed with 'not'
    pub is_negated: bool,
    /// The operand to potentially negate
    pub operand: UnaryOperand,
}
const _: () = assert!(size_of::<UnaryExpr>() == 20);

/// Represents a comparison expression
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
/// ```kumi
/// @if option(MAX_THREADS) > 8 { ... }
/// @if version == 2 { ... }
/// @if arch(x86_64) { ... }  // Unary expression without comparison
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ComparisonExpr {
    pub base: NodeBase,
    /// Index of Left-hand side expression
    pub left_idx: u32,
    /// Comparison operator (if binary)
    pub op: Option<ComparisonOperator>,
    /// Index of the right-hand side expression (if binary)
    pub right_idx: Option<u32>,
}
const _: () = assert!(size_of::<ComparisonExpr>() == 24);

/// Represents a logical expression (AND/OR of comparisons)
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
/// ```kumi
/// @if platform(windows) and arch(x86_64) { ... }
/// @if config(debug) or option(FORCE_LOGGING) { ... }
/// @if a and b and c { ... }
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct LogicalExpr {
    pub base: NodeBase,
    /// Index of the start of comparison operands
    pub operand_start_idx: u32,
    /// Operator: `and` or `or`
    pub op: LogicalOperator,
    /// Index of the end of comparison operands
    pub operand_end_idx: u32,
}
const _: () = assert!(size_of::<LogicalExpr>() == 20);

/// Top-level condition type used in if statements
///
/// A condition can be a logical expression, comparison, or simple unary expression.
///
/// Grammar:
/// ```ebnf
/// Condition = LogicalExpr ;
/// LogicalExpr = ComparisonExpr { ( "and" | "or" ) ComparisonExpr } ;
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Condition {
    /// a and b, x or y or z
    LogicalExpr(LogicalExpr),
    /// a > 5, platform(windows)
    ComparisonExpr(ComparisonExpr),
    /// not feature(x), platform(linux)
    UnaryExpr(UnaryExpr),
}
const _: () = assert!(size_of::<Condition>() == 24);

/// Iterable expression for for-loops
///
/// Grammar:
/// ```ebnf
/// Iterable = List | Range | FunctionCall ;
/// ```
///
/// Examples:
/// ```kumi
/// @for x in [a, b, c] { ... }       // List
/// @for i in 0..10 { ... }           // Range
/// @for file in glob("*.cpp") { ... } // FunctionCall
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Iterable {
    /// Explicit list of values
    List(List),
    /// Numeric range
    Range(Range),
    /// Function returning iterable (e.g., glob)
    FunctionCall(FunctionCall),
}
const _: () = assert!(size_of::<Iterable>() == 24);

//===----------------------------------------------------------------------===//
// Properties
//===----------------------------------------------------------------------===//

/// Represents a property assignment (key-value pair)
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
/// ```kumi
/// type: executable;                          // Single value
/// sources: "main.cpp", "utils.cpp";          // Multiple values
/// cxx_standard: 20;                          // Integer value
/// warnings: "all", "extra", "pedantic";      // Multiple string values
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Property {
    pub base: NodeBase,
    /// Index of the function name (type, sources, defines, ...)
    pub name_idx: u32,
    /// Index of the start of property values (one or more)
    pub value_start_idx: u32,
    /// Index of the end of property values (one or more)
    pub value_end_idx: u32,
}
const _: () = assert!(size_of::<Property>() == 20);

//===----------------------------------------------------------------------===//
// Dependencies
//===----------------------------------------------------------------------===//

/// Dependency value type
///
/// Dependencies can reference version strings, system packages,
/// git repositories, or local paths.
///
/// Grammar:
/// ```ebnf
/// DependencyValue = String | Identifier | FunctionCall ;
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum DependencyValue<'a> {
    /// Version string: "1.0.0", "^2.3.4"
    String(&'a str),
    /// git(...), path(...), system
    FunctionCall(FunctionCall),
}
const _: () = assert!(size_of::<DependencyValue>() == 24);

/// Represents a single dependency specification
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
/// ```kumi
/// fmt: "10.2.1";    // Simple version
/// opengl?: system; // Optional system dependency
/// imgui: git("https://github.com/...") {
///     tag: "v1.90";
/// };
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DependencySpec<'a> {
    pub base: NodeBase,
    /// Index of the dependency name (package identifier)
    pub name_idx: u32,
    /// Version, git URL, path, or system
    pub value: DependencyValue<'a>,
    /// Index of the start of additional options such as `tag`
    pub option_start_idx: u32,
    /// Index of the end of additional options
    pub option_end_idx: u32,
    /// True if suffixed with `?`
    pub is_optional: bool,
}
const _: () = assert!(size_of::<DependencySpec>() == 48);

//===----------------------------------------------------------------------===//
// Options
//===----------------------------------------------------------------------===//

/// Represents a build option specification
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
/// ```kumi
/// BUILD_TESTS: true;
/// MAX_THREADS: 8 {
///     min: 1;
///     max: 128;
/// };
/// LOG_LEVEL: "info" {
///     choices: "debug", "info", "warning", "error";
/// };
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct OptionSpec<'a> {
    pub base: NodeBase,
    /// Index of the option name
    pub name_idx: u32,
    /// Default value
    pub default_value: Value<'a>,
    /// index of the start of constraints such as `min`
    pub constraint_start_idx: u32,
    /// index of the end of constraints such as `min`
    pub constraint_end_idx: u32,
}
const _: () = assert!(size_of::<OptionSpec>() == 48);

//===----------------------------------------------------------------------===//
// Top-Level Declarations
//===----------------------------------------------------------------------===//

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Statement {
    ProjectDecl(ProjectDecl),
    WorkspaceDecl(WorkspaceDecl),
    TargetDecl(TargetDecl),
    DependenciesDecl(DependenciesDecl),
    OptionsDecl(OptionsDecl),
    MixinDecl(MixinDecl),
    ProfileDecl(ProfileDecl),
    InstallDecl(InstallDecl),
    PackageDecl(PackageDecl),
    ScriptDecl(ScriptDecl),
    VisibilityBlock(VisibilityBlock),
    IfStmt(IfStmt),
    ForStmt(ForStmt),
    LoopControlStmt(LoopControlStmt),
    DiagnosticStmt(DiagnosticStmt),
    Property(Property),
}
const _: () = assert!(size_of::<Statement>() == 48);

/// Represents a project declaration
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
/// ```kumi
/// project myapp {
///     version: "1.0.0";
///     cpp: 23;
///     license: "MIT";
/// }
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ProjectDecl {
    pub base: NodeBase,
    /// Index of the project name
    pub name_idx: u32,
    /// Index of the start of project configuration properties
    pub property_start_idx: u32,
    /// Index of the end of project configuration properties
    pub property_end_idx: u32,
}
const _: () = assert!(size_of::<ProjectDecl>() == 20);

/// Represents a workspace declaration
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
/// ```kumi
/// workspace {
///     build-dir: "build";
///     output-dir: "dist";
///     warnings-as-errors: true;
/// }
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct WorkspaceDecl {
    pub base: NodeBase,
    /// Index of the start of workspace configuration
    pub property_start_idx: u32,
    /// Index of the end of workspace configuration
    pub property_end_idx: u32,
}
const _: () = assert!(size_of::<WorkspaceDecl>() == 16);

/// Represents a target declaration
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
/// ```kumi
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
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TargetDecl {
    pub base: NodeBase,
    /// Index of the target name
    pub name_idx: u32,
    /// Index of the start of mixins to apply
    pub mixin_start_idx: u32,
    /// Index of the end of mixins to apply
    pub mixin_end_idx: u32,
    /// Index of the start of target body as `properties`
    pub body_start_idx: u32,
    /// Index of the end of target body as `properties`
    pub body_end_idx: u32,
}
const _: () = assert!(size_of::<TargetDecl>() == 28);

/// Represents a dependencies declaration
///
/// Declares all external dependencies required by the project.
///
/// Grammar:
/// ```ebnf
/// DependenciesDecl = "dependencies" "{" { DependencySpec } "}" ;
/// ```
///
/// Example:
/// ```kumi
/// dependencies {
///     fmt: "10.2.1";
///     spdlog: "1.12.0";
///     opengl?: system;
///     imgui: git("https://github.com/ocornut/imgui") {
///         tag: "v1.90";
///     };
/// }
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DependenciesDecl {
    pub base: NodeBase,
    /// Index of the start of list of dependency specifications
    pub dep_start_idx: u32,
    /// Index of the end of list of dependency specifications
    pub dep_end_idx: u32,
}
const _: () = assert!(size_of::<DependenciesDecl>() == 16);

/// Represents an options declaration
///
/// Defines user-configurable build options with default values and constraints.
///
/// Grammar:
/// ```ebnf
/// OptionsDecl = "options" "{" { OptionSpec } "}" ;
/// ```
///
/// Example:
/// ```kumi
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
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct OptionsDecl {
    pub base: NodeBase,
    /// Index of the start of build option specifications
    pub option_start_idx: u32,
    /// Index of the end of build option specifications
    pub option_end_idx: u32,
}
const _: () = assert!(size_of::<OptionsDecl>() == 16);

/// Represents a mixin declaration
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
/// ```kumi
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
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct MixinDecl {
    pub base: NodeBase,
    /// Index of the mixin name
    pub name_idx: u32,
    /// Index of the start of mixin body
    pub body_start_idx: u32,
    /// Index of the end of mixin body
    pub body_end_idx: u32,
}
const _: () = assert!(size_of::<MixinDecl>() == 20);

/// Represents a profile declaration
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
/// ```kumi
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
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ProfileDecl {
    pub base: NodeBase,
    /// Index of the profile name (debug, release, ...)
    pub name_idx: u32,
    /// Index of the start of mixins to apply
    pub mixin_start_idx: u32,
    /// Index of the end of mixins to apply
    pub mixin_end_idx: u32,
    /// Index of the start of profile configuration
    pub property_start_idx: u32,
    /// Index of the end of profile configuration
    pub property_end_idx: u32,
}
const _: () = assert!(size_of::<ProfileDecl>() == 28);

/// Represents an install declaration
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
/// ```kumi
/// install {
///     destination: "/usr/local/bin";
///     permissions: "755";
///     targets: myapp, mylib;
/// }
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct InstallDecl {
    pub base: NodeBase,
    /// Index of the start of installation configuration
    pub property_start_idx: u32,
    /// Index of the end of installation configuration
    pub property_end_idx: u32,
}
const _: () = assert!(size_of::<InstallDecl>() == 16);

/// Represents a package declaration
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
/// ```kumi
/// package {
///     format: "deb", "rpm";
///     maintainer: "user@example.com";
///     description: "My awesome application";
///     license: "MIT";
/// }
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PackageDecl {
    pub base: NodeBase,
    /// Index of the start of package configuration
    pub property_start_idx: u32,
    /// Index of the end of package configuration
    pub property_end_idx: u32,
}
const _: () = assert!(size_of::<PackageDecl>() == 16);

/// Represents a scripts declaration
///
/// Defines custom build scripts and hooks that run at various
/// stages of the build process (pre-build, post-build, ...).
///
/// Grammar:
/// ```ebnf
/// ScriptDecl = "script" "{" { PropertyAssignment } "}" ;
/// ```
///
/// Example:
/// ```kumi
/// script {
///     name: "generate-headers";
///     command: "python gen.py";
///     phase: "prebuild";
/// }
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ScriptDecl {
    pub base: NodeBase,
    /// Index of the start of script properties
    pub property_start_idx: u32,
    /// Index of the end of script properties
    pub property_end_idx: u32,
}
const _: () = assert!(size_of::<ScriptDecl>() == 16);

//===----------------------------------------------------------------------===//
// Visibility Blocks
//===----------------------------------------------------------------------===//

/// Visibility modifier for target properties
///
/// Controls how properties propagate to dependent targets, following
/// CMake's model of transitive dependencies.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Visibility {
    /// Visible to this target and all consumers
    Public,
    /// Visible only to this target
    Private,
    /// Visible only to consumers (not this target)
    Interface,
}

/// Represents a visibility block within a target or mixin
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
/// ```kumi
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
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct VisibilityBlock {
    pub base: NodeBase,
    /// Visibility level
    pub visibility: Visibility,
    /// Index of the start of properties with this visibility
    pub property_start_idx: u32,
    /// Index of the end of properties with this visibility
    pub property_end_idx: u32,
}
const _: () = assert!(size_of::<VisibilityBlock>() == 20);

//===----------------------------------------------------------------------===//
// Control Flow Statements
//===----------------------------------------------------------------------===//

/// Represents a conditional statement (if/else-if/else)
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
/// ```kumi
/// @if platform(windows) {
///     sources: "win32.cpp";
/// } @else-if platform(macos) {
///     sources: "macos.cpp";
/// } @else {
///     sources: "linux.cpp";
/// }
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct IfStmt {
    pub base: NodeBase,
    /// Condition to evaluate
    pub condition: Condition,
    /// Index of start of then block statements
    pub then_start_idx: u32,
    /// Index of end of then block statements
    pub then_end_idx: u32,
    /// Index of start of else block statements
    pub else_start_idx: u32,
    /// Index of end of else block statements
    pub else_end_idx: u32,
}
const _: () = assert!(size_of::<IfStmt>() == 48);

/// Represents a for-loop statement
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
/// ```kumi
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
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ForStmt {
    pub base: NodeBase,
    /// Index of the loop variable name
    pub variable_name_idx: u32,
    /// Collection to iterate over
    pub iterable: Iterable,
    /// Index of the start of loop body statements
    pub body_start_idx: u32,
    /// Index of the end of loop body statements
    pub body_end_idx: u32,
}
const _: () = assert!(size_of::<ForStmt>() == 44);

/// Loop control operation type
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LoopControl {
    /// Exit loop immediately: `@break;`
    Break,
    /// Skip to next iteration: `@continue;`
    Continue,
}

/// Represents a loop control statement
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
/// ```kumi
/// @for file in glob("src/**/*.cpp") {
///     @if file.contains("deprecated") {
///         @continue;
///     }
///     sources: ${file};
/// }
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct LoopControlStmt {
    pub base: NodeBase,
    /// Break or continue
    pub control: LoopControl,
}
const _: () = assert!(size_of::<LoopControlStmt>() == 12);

//===----------------------------------------------------------------------===//
// Diagnostic and Import Statements
//===----------------------------------------------------------------------===//

/// Diagnostic message severity level
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DiagnosticLevel {
    /// Error: stops build
    Error,
    /// Warning: continues build
    Warning,
    /// Info: informational message
    Info,
    /// Debug: verbose debugging message (shown with --verbose)
    Debug,
}

/// Represents a diagnostic message statement
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
/// ```kumi
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
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DiagnosticStmt {
    pub base: NodeBase,
    /// Message severity
    pub level: DiagnosticLevel,
    /// Index of the diagnostics message text
    pub message_idx: u32,
}
const _: () = assert!(size_of::<DiagnosticStmt>() == 16);

//===----------------------------------------------------------------------===//
// Root AST
//===----------------------------------------------------------------------===//

/// Root AST node representing a complete Kumi build file
///
/// The AST uses a type-separated storage pattern for memory efficiency and
/// cache locality. Instead of storing child nodes directly in parent nodes,
/// all nodes of the same type are stored in contiguous vectors and reference
/// them via start/end indices.
use bumpalo::Bump;

#[derive(Debug, Default)]
pub struct Ast<'a> {
    /// Arena allocator for all string data
    pub string_storage: Bump,

    //===----------------------------------------------------------------------===//
    // Side vectors (Structure of Arrays)
    //===----------------------------------------------------------------------===//
    /// Storage for all ComparisonExpr nodes
    pub all_comparison_exprs: Vec<ComparisonExpr>,
    /// Storage for all DependencySpec nodes
    pub all_dependencies: Vec<DependencySpec<'a>>,
    /// Storage for all FunctionCall nodes
    pub all_function_calls: Vec<FunctionCall>,
    /// Storage for all LogicalExpr nodes
    pub all_logical_exprs: Vec<LogicalExpr>,
    /// Storage for all OptionSpec nodes
    pub all_options: Vec<OptionSpec<'a>>,
    /// Storage for all Property nodes
    pub all_properties: Vec<Property>,
    /// Storage for all Statement nodes
    pub all_statements: Vec<Statement>,
    /// Storage for all UnaryExpr nodes
    pub all_unary_exprs: Vec<UnaryExpr>,
    /// Storage for all UnaryOperand nodes
    pub all_unary_operands: Vec<UnaryOperand>,
    /// Storage for all Value nodes (literals and identifiers)
    pub all_values: Vec<Value<'a>>,
    /// Storage for all strings referenced by the AST
    pub all_strings: Vec<&'a str>,

    /// Top-level statements in the build file
    ///
    /// This is the main entry point for AST traversal. Contains all root-level
    /// declarations (`project`, `workspace`, `targets`, `dependencies`, ...) in
    /// parse order. This is used to iterate over the file's top-level structure.
    pub statements: Vec<Statement>,

    /// References the path to the source file this AST represents. Used for
    /// error reporting and import resolution.
    pub file_path: &'a str,
    /// Storage for parse and semantic errors encountered
    pub errors: Vec<crate::diagnostics::Diagnostic>,
}

impl<'a> Ast<'a> {
    pub fn new(file_path: &'a str) -> Self {
        Self {
            file_path,
            errors: Vec::new(),
            ..Default::default()
        }
    }

    //===----------------------------------------------------------------------===//
    // Access helpers
    //===----------------------------------------------------------------------===//

    /// Retrieves a slice of dependency specifications
    pub fn get_dependencies(&self, start_idx: u32, end_idx: u32) -> &[DependencySpec<'a>] {
        &self.all_dependencies[start_idx as usize..end_idx as usize]
    }

    /// Retrieves a slice of option specifications
    pub fn get_options(&self, start_idx: u32, end_idx: u32) -> &[OptionSpec<'a>] {
        &self.all_options[start_idx as usize..end_idx as usize]
    }

    /// Retrieves a slice of properties
    pub fn get_properties(&self, start_idx: u32, end_idx: u32) -> &[Property] {
        &self.all_properties[start_idx as usize..end_idx as usize]
    }

    /// Retrieves a slice of statements (for nested blocks)
    pub fn get_statements(&self, start_idx: u32, end_idx: u32) -> &[Statement] {
        &self.all_statements[start_idx as usize..end_idx as usize]
    }

    /// Retrieves a string by index
    pub fn get_string(&self, idx: u32) -> &'a str {
        self.all_strings[idx as usize]
    }

    /// Retrieves a value by index
    pub fn get_value(&self, idx: u32) -> &Value<'a> {
        &self.all_values[idx as usize]
    }
}
