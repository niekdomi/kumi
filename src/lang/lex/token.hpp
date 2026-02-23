/// @file token.hpp
/// @brief Token definitions
///
/// @see Lexer for tokenization implementation
/// @see TokenType for all supported token types

#pragma once

#include <cstdint>
#include <string_view>

namespace kumi::lang {

/// @brief All token types recognized by the lexer
///
/// Tokens are grouped by category:
/// - Top-level declarations (`project`, `target`, ...)
/// - Visibility modifiers (`public`, `private`, `interface`)
/// - Control flow (`@if`, `@for`, `@break`, `@continue`)
/// - Diagnostic directives (`@error`, `@warning`, `@info`)
/// - Logical operators (`and`, `or`, `not`)
/// - Operators and punctuation
/// - Literals (`strings`, `numbers`, `booleans`, ...)
/// - Special tokens (`EOF`)
enum class TokenType : std::uint8_t
{
    //===-----------------------------------------------------------------===//
    // Top-Level Declarations
    //===-----------------------------------------------------------------===//

    PROJECT,      ///< `project myapp { }`       - Project metadata and configuration
    WORKSPACE,    ///< `workspace { }`           - Multi-project workspace configuration
    TARGET,       ///< `target mylib { }`        - Build target (executable, library, etc.)
    DEPENDENCIES, ///< `dependencies { }`        - External dependencies
    OPTIONS,      ///< `options { }`             - User-configurable build options
    MIXIN,        ///< `mixin strict { }`        - Reusable property sets
    PROFILE,      ///< `profile release { }`     - Named build configuration profile
    AT_IMPORT,    ///< `@import "file.kumi";`    - Import another Kumi configuration file
    INSTALL,      ///< `install { }`             - Installation configuration
    PACKAGE,      ///< `package { }`             - Packaging and publishing
    SCRIPTS,      ///< `scripts { }`             - Custom build hooks
    WITH,         ///< `with`                    - Mixin (e.g., `target myapp with strict { }`)

    //===-----------------------------------------------------------------===//
    // Visibility Modifiers
    //===-----------------------------------------------------------------===//

    PUBLIC,    ///< `public { }`    - Properties visible to target and dependents
    PRIVATE,   ///< `private { }`   - Properties visible only to this target
    INTERFACE, ///< `interface { }` - Properties visible only to dependents

    //===-----------------------------------------------------------------===//
    // Control Flow
    //===-----------------------------------------------------------------===//

    AT_IF,       ///< `@if condition { }`      - Conditional branch
    AT_ELSE_IF,  ///< `@else-if condition { }` - Else-if branch
    AT_ELSE,     ///< `@else { }`              - Else branch
    AT_FOR,      ///< `@for item in list { }`  - Loop over iterable
    IN,          ///< `in`                     - Iterator keyword in for loops
    AT_BREAK,    ///< `@break;`                - Exit loop immediately
    AT_CONTINUE, ///< `@continue;`             - Skip to next loop iteration

    //===-----------------------------------------------------------------===//
    // Diagnostic Directives
    //===-----------------------------------------------------------------===//

    AT_ERROR,   ///< `@error "msg";`   - Emit build error and halt
    AT_WARNING, ///< `@warning "msg";` - Emit build warning
    AT_INFO,    ///< `@info "msg";`    - Emit informational message
    AT_DEBUG,   ///< `@debug "msg";`   - Emit debug message (shown with `--verbose` flag)

    //===-----------------------------------------------------------------===//
    // Logical Operators
    //===-----------------------------------------------------------------===//

    AND, ///< `and` - Logical AND operator
    OR,  ///< `or`  - Logical OR operator
    NOT, ///< `not` - Logical NOT operator

    //===-----------------------------------------------------------------===//
    // Operators and Punctuation
    //===-----------------------------------------------------------------===//

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

    //===-----------------------------------------------------------------===//
    // Literals
    //===-----------------------------------------------------------------===//

    IDENTIFIER, ///< Identifier - `myapp`, `foo_bar`, `my-lib`
    STRING,     ///< String literal - `"hello world"`, `"path/to/file"`
    NUMBER,     ///< Integer literal - `123`, `42`, `0`
    TRUE,       ///< Boolean literal - `true`
    FALSE,      ///< Boolean literal - `false`
    COMMENT,    ///< TODO: REMOVE

    //===-----------------------------------------------------------------===//
    // Special
    //===-----------------------------------------------------------------===//

    END_OF_FILE, ///< End of file marker
};

/// @brief Represents a single lexical token
///
/// Contains the token type, its textual value, and source location information
/// for error reporting and debugging.
struct Token final
{
    // TODO(domi): Handle comments ()
    std::string_view value;   ///< Textual value. String literals include quotes (e.g., `"hello"`)
    std::uint32_t position{}; ///< Starting position in source text
    TokenType type{};         ///< Type of the token
};

static_assert(sizeof(Token) == 24);

} // namespace kumi::lang

