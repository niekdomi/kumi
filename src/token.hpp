/// @file token.hpp
/// @brief Token definitions
///
/// @see Lexer for tokenization implementation
/// @see TokenType for all supported token types

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace kumi {

/// @brief TokenType represents all possible token types
enum class TokenType : std::uint8_t
{
    // Keywords - Top Level
    PROJECT,      ///< `project myapp { }`   - Defines project name and settings
    WORKSPACE,    ///< `workspace { }`       - Defines workspace-wide configuration
    TARGET,       ///< `target mylib { }`    - Defines a build target (executable, library, ...)
    DEPENDENCIES, ///< `dependencies { }`    - Lists external dependencies
    OPTIONS,      ///< `options { }`         - Defines build options and cache variables
    GLOBAL,       ///< `global { }`          - Global settings applied to all targets
    MIXIN,        ///< `mixin common { }`    - Reusable property sets
    PRESET,       ///< `preset embedded { }` - Build presets with predefined configurations
    FEATURES,     ///< `features { }`        - Optional features that can be toggled
    TESTING,      ///< `testing { }`         - Testing framework configuration
    INSTALL,      ///< `install { }`         - Installation rules and destinations
    PACKAGE,      ///< `package { }`         - Packaging configuration
    SCRIPTS,      ///< `scripts { }`         - Custom build scripts and hooks
    TOOLCHAIN,    ///< `toolchain arm { }`   - Compiler toolchain configuration
    ROOT,         ///< `:root { }`           - Custom property definitions

    // Keywords - Visibility
    PUBLIC,    ///< `public { }`    - Properties visible to this target and all dependents
    PRIVATE,   ///< `private { }`   - Properties visible only to this target
    INTERFACE, ///< `interface { }` - Properties visible only to dependents

    // Keywords - Properties
    TYPE,     ///< `type: executable;`       - Target type (executable, static-lib, shared-lib, ...)
    SOURCES,  ///< `sources: "*.cpp";`       - Source files for compilation
    HEADERS,  ///< `headers: "*.hpp";`       - Header files
    DEPENDS,  ///< `depends: fmt, spdlog;`   - Target dependencies
    APPLY,    ///< `apply: mixin-name;`      - Apply a mixin to current scope
    INHERITS, ///< `inherits: base-target;`  - Inherit properties from another target
    EXTENDS,  ///< `extends base-target { }` - Extend a target with additional properties
    EXPORT,   ///< `export: true;`           - Export target for external use
    IMPORT,   ///< `import: mylib;`          - Import a library or module

    // Keywords - Control Flow
    IF,             ///< `@if condition { }`      - If branch
    ELSE_IF,        ///< `@else-if condition { }` - Else-if branch
    ELSE,           ///< `@else { }`              - Else branch
    FOR,            ///< `@for item in list { }`  - Loop iteration
    IN,             ///< `in`                     - Used in for loops: `@for item in list`
    BREAK,          ///< `@break;`                - Exit loop immediately
    CONTINUE,       ///< `@continue;`             - Skip to next loop iteration
    ERROR,          ///< `@error "message";`      - Emit error message (red) and stop build
    WARNING,        ///< `@warning "message";`    - Emit warning message (yellow)
    INFO,           ///< `@info "message";`       - Emit informational message (cyan)
    IMPORT_KEYWORD, ///< `@import "file.kumi";`   - Import another Kumi file
    APPLY_KEYWORD,  ///< `@apply profile(x) { }`  - Apply profile with overrides

    // Keywords - Logical
    AND, ///< `and` - Logical AND operator
    OR,  ///< `or`  - Logical OR operator
    NOT, ///< `not` - Logical NOT operator

    // Keywords - Functions
    PLATFORM,    ///< `platform(windows)`          - Platform detection (windows, linux, macos, ...)
    ARCH,        ///< `arch(x86_64)`               - Architecture detection (x86_64, arm64, ...)
    COMPILER,    ///< `compiler(gcc)`              - Compiler detection (gcc, clang, msvc, ...)
    CONFIG,      ///< `config(debug)`              - Build configuration (debug, release, ...)
    OPTION,      ///< `option(BUILD_TESTS)`        - Check build option value
    FEATURE,     ///< `feature(vulkan)`            - Check if feature is enabled
    HAS_FEATURE, ///< `has-feature(avx2)`          - Check for compiler/CPU feature support
    EXISTS,      ///< `exists("path/file")`        - Check if file or directory exists
    VAR,         ///< `var(--variable)`            - Variable reference from :root
    GLOB,        ///< `glob("src/**/*.cpp")`       - File globbing pattern
    GIT,         ///< `git("https://...")`         - Git repository dependency
    URL,         ///< `url("https://...")`         - URL download dependency
    SYSTEM,      ///< `system { required: true; }` - System dependency

    // Operators & Punctuation
    LEFT_BRACE,    ///< `{`   - Opening brace for blocks
    RIGHT_BRACE,   ///< `}`   - Closing brace for blocks
    LEFT_BRACKET,  ///< `[`   - Opening bracket for lists
    RIGHT_BRACKET, ///< `]`   - Closing bracket for lists
    LEFT_PAREN,    ///< `(`   - Opening parenthesis for function calls
    RIGHT_PAREN,   ///< `)`   - Closing parenthesis for function calls
    COLON,         ///< `:`   - Property assignment separator
    SEMICOLON,     ///< `;`   - Statement terminator
    COMMA,         ///< `,`   - List item separator
    DOT,           ///< `.`   - Member access or decimal point
    RANGE,         ///< `..`  - Range operator (e.g., `1..10`)
    ELLIPSIS,      ///< `...` - Variadic or spread operator
    QUESTION,      ///< `?`   - Optional dependency marker (e.g., `vulkan?: "1.3"`)
    EXCLAMATION,   ///< `!`   - Negation or required marker
    ASSIGN,        ///< `=`   - Assignment operator
    EQUAL,         ///< `==`  - Equality comparison
    NOT_EQUAL,     ///< `!=`  - Inequality comparison
    LESS,          ///< `<`   - Less than comparison
    LESS_EQUAL,    ///< `<=`  - Less than or equal comparison
    GREATER,       ///< `>`   - Greater than comparison
    GREATER_EQUAL, ///< `>=`  - Greater than or equal comparison
    DOLLAR,        ///< `$`   - Variable interpolation prefix (e.g., `${--var}`)
    MINUS_MINUS,   ///< `--`  - Custom property prefix (e.g., `--version`)
    ARROW,         ///< `->`  - Arrow operator

    // Literals
    IDENTIFIER, ///< Identifier (e.g., `foo`, `bar_baz`)
    STRING,     ///< String literal (e.g., `"hello"`)
    NUMBER,     ///< Numeric literal (e.g., `123`, `3.14`)
    TRUE,       ///< Boolean literal `true`
    FALSE,      ///< Boolean literal `false`

    // Special
    END_OF_FILE ///< End of file marker
};

/// @brief Represents a single lexical token
///
/// Contains the token type, its textual value, and source location information
/// for error reporting and debugging.
struct Token final
{
    TokenType type;     ///< Type of the token
    std::string value;  ///< Textual value
    std::size_t line;   ///< Line number in source file
    std::size_t column; ///< Column number in source file
};

} // namespace kumi
