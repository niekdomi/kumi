/// @file token.hpp
/// @brief Token definitions
///
/// @see Lexer for tokenization implementation
/// @see TokenType for all supported token types

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace kumi {

/// @brief All token types recognized by the lexer
///
/// Tokens are grouped by category:
/// - Top-level declarations (`project`, `target`, ...)
/// - Visibility modifiers (`public`, `private`, `interface`)
/// - Control flow (`@if`, `@for`, `@break`, `@continue`)
/// - Diagnostic directives (`@error`, `@warning`, `@info`)
/// - Logical operators (`and`, `or`, `not`)
/// - Operators and punctuation
/// - Literals (`strings`, `numbers`, `booleans`, `identifiers`)
/// - Special tokens (`EOF`)
enum class TokenType : std::uint8_t
{
    //===---------------------------------------------------------------------===//
    // Top-Level Declarations
    //===---------------------------------------------------------------------===//

    PROJECT,      ///< `project myapp { }`       - Project metadata and configuration
    WORKSPACE,    ///< `workspace { }`           - Multi-project workspace configuration
    TARGET,       ///< `target mylib { }`        - Build target (executable, library, etc.)
    DEPENDENCIES, ///< `dependencies { }`        - External dependencies
    OPTIONS,      ///< `options { }`             - User-configurable build options
    GLOBAL,       ///< `global { }`              - Global settings for all targets
    MIXIN,        ///< `mixin strict { }`        - Reusable property sets
    PROFILE,      ///< `profile release { }`     - Named build configuration profile
    AT_IMPORT,    ///< `@import "file.kumi";`    - Import another Kumi configuration file
    INSTALL,      ///< Reserved: `install { }`   - Installation configuration
    PACKAGE,      ///< Reserved: `package { }`   - Packaging and publishing
    SCRIPTS,      ///< Reserved: `scripts { }`   - Custom build hooks
    TOOLCHAIN,    ///< Reserved: `toolchain { }` - Compiler toolchain setup

    //===---------------------------------------------------------------------===//
    // Visibility Modifiers
    //===---------------------------------------------------------------------===//

    PUBLIC,    ///< `public { }`    - Properties visible to target and dependents
    PRIVATE,   ///< `private { }`   - Properties visible only to this target
    INTERFACE, ///< `interface { }` - Properties visible only to dependents

    //===---------------------------------------------------------------------===//
    // Control Flow
    //===---------------------------------------------------------------------===//

    AT_IF,       ///< `@if condition { }`      - Conditional branch
    AT_ELSE_IF,  ///< `@else-if condition { }` - Else-if branch
    AT_ELSE,     ///< `@else { }`              - Else branch
    AT_FOR,      ///< `@for item in list { }`  - Loop over iterable
    IN,          ///< `in`                     - Iterator keyword in for loops
    AT_BREAK,    ///< `@break;`                - Exit loop immediately
    AT_CONTINUE, ///< `@continue;`             - Skip to next loop iteration

    //===---------------------------------------------------------------------===//
    // Diagnostic Directives
    //===---------------------------------------------------------------------===//

    AT_ERROR,   ///< Reserved: `@error "msg";`   - Emit build error and halt
    AT_WARNING, ///< Reserved: `@warning "msg";` - Emit build warning
    AT_INFO,    ///< Reserved: `@info "msg";`    - Emit informational message

    //===---------------------------------------------------------------------===//
    // Logical Operators
    //===---------------------------------------------------------------------===//

    AND, ///< `and` - Logical AND operator
    OR,  ///< `or`  - Logical OR operator
    NOT, ///< `not` - Logical NOT operator

    //===---------------------------------------------------------------------===//
    // Operators and Punctuation
    //===---------------------------------------------------------------------===//

    // Braces, Brackets, and Parentheses
    LEFT_BRACE,    ///< `{` - Opening brace for blocks
    RIGHT_BRACE,   ///< `}` - Closing brace for blocks
    LEFT_BRACKET,  ///< `[` - Opening bracket for explicit lists
    RIGHT_BRACKET, ///< `]` - Closing bracket for explicit lists
    LEFT_PAREN,    ///< `(` - Opening parenthesis for function calls
    RIGHT_PAREN,   ///< `)` - Closing parenthesis for function calls

    // Delimiters
    COLON,     ///< `:` - Property assignment separator
    SEMICOLON, ///< `;` - Statement terminator
    COMMA,     ///< `,` - List item separator

    // Special Operators
    QUESTION, ///< `?`  - Optional dependency marker (e.g., `vulkan?: "1.3"`)
    DOLLAR,   ///< `$`  - String interpolation prefix (e.g., `${project.version}`)
    RANGE,    ///< `..` - Range operator for loops (e.g., `0..10`)

    // Comparison Operators
    EQUAL,         ///< `==` - Equality comparison
    NOT_EQUAL,     ///< `!=` - Inequality comparison
    LESS,          ///< `<`  - Less than comparison
    LESS_EQUAL,    ///< `<=` - Less than or equal comparison
    GREATER,       ///< `>`  - Greater than comparison
    GREATER_EQUAL, ///< `>=` - Greater than or equal comparison

    //===---------------------------------------------------------------------===//
    // Literals
    //===---------------------------------------------------------------------===//

    IDENTIFIER, ///< Identifier - `myapp`, `foo_bar`, `my-lib`
    STRING,     ///< String literal - `"hello world"`, `"path/to/file"`
    NUMBER,     ///< Integer literal - `123`, `42`, `0`
    TRUE,       ///< Boolean literal - `true`
    FALSE,      ///< Boolean literal - `false`

    //===---------------------------------------------------------------------===//
    // Special
    //===---------------------------------------------------------------------===//

    END_OF_FILE ///< End of file marker
};

/// @brief Represents a single lexical token
///
/// Contains the token type, its textual value, and source location information
/// for error reporting and debugging.
struct Token final
{
    std::string_view value; ///< Textual value. String literals include quotes (e.g., `"hello"`)
    std::size_t position;   ///< Starting position in source text
    TokenType type;         ///< Type of the token
};

} // namespace kumi
