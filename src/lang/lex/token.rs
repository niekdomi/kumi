use std::mem::size_of;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
/// All token types recognized by the lexer
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
pub enum TokenType {
    //===-----------------------------------------------------------------===//
    // Top-Level Declarations
    //===-----------------------------------------------------------------===//
    /// `project myapp { }`       - Project metadata and configuration
    Project,
    /// `workspace { }`           - Multi-project workspace configuration
    Workspace,
    /// `target mylib { }`        - Build target (executable, library, etc.)
    Target,
    /// `dependencies { }`        - External dependencies
    Dependencies,
    /// `options { }`             - User-configurable build options
    Options,
    /// `mixin strict { }`        - Reusable property sets
    Mixin,
    /// `profile release { }`     - Named build configuration profile
    Profile,
    /// `install { }`             - Installation configuration
    Install,
    /// `package { }`             - Packaging and publishing
    Package,
    /// `script { }`             - Custom build hooks
    Script,
    /// `with`                    - Mixin (e.g., `target myapp with strict { }`)
    With,

    //===-----------------------------------------------------------------===//
    // Visibility Modifiers
    //===-----------------------------------------------------------------===//
    /// `public { }`    - Properties visible to target and dependents
    Public,
    /// `private { }`   - Properties visible only to this target
    Private,
    /// `interface { }` - Properties visible only to dependents
    Interface,

    //===-----------------------------------------------------------------===//
    // Control Flow
    //===-----------------------------------------------------------------===//
    /// `@if condition { }`      - Conditional branch
    AtIf,
    /// `@else-if condition { }` - Else-if branch
    AtElseIf,
    /// `@else { }`              - Else branch
    AtElse,
    /// `@for item in list { }`  - Loop over iterable
    AtFor,
    /// `in`                     - Iterator keyword in for loops
    In,
    /// `@break;`                - Exit loop immediately
    AtBreak,
    /// `@continue;`             - Skip to next loop iteration
    AtContinue,

    //===-----------------------------------------------------------------===//
    // Diagnostic Directives
    //===-----------------------------------------------------------------===//
    /// `@error "msg";`   - Emit build error and halt
    AtError,
    /// `@warning "msg";` - Emit build warning
    AtWarning,
    /// `@info "msg";`    - Emit informational message
    AtInfo,
    /// `@debug "msg";`   - Emit debug message (shown with `--verbose` flag)
    AtDebug,

    //===-----------------------------------------------------------------===//
    // Logical Operators
    //===-----------------------------------------------------------------===//
    /// `and` - Logical AND operator
    And,
    /// `or`  - Logical OR operator
    Or,
    /// `not` - Logical NOT operator
    Not,

    //===-----------------------------------------------------------------===//
    // Operators and Punctuation
    //===-----------------------------------------------------------------===//
    /// `{` - Opening brace for blocks
    LeftBrace,
    /// `}` - Closing brace for blocks
    RightBrace,
    /// `[` - Opening bracket for explicit lists
    LeftBracket,
    /// `]` - Closing bracket for explicit lists
    RightBracket,
    /// `(` - Opening parenthesis for function calls
    LeftParen,
    /// `)` - Closing parenthesis for function calls
    RightParen,

    /// `:` - Property assignment separator
    Colon,
    /// `;` - Statement terminator
    Semicolon,
    /// `,` - List item separator
    Comma,

    /// `?`  - Optional dependency marker (e.g., `vulkan?: "1.3"`)
    Question,
    /// `$`  - String interpolation prefix (e.g., `${project.version}`)
    Dollar,
    /// `..` - Range operator for loops (e.g., `0..10`)
    Range,

    /// `==` - Equality comparison
    Equal,
    /// `!=` - Inequality comparison
    NotEqual,
    /// `<`  - Less than comparison
    Less,
    /// `<=` - Less than or equal comparison
    LessEqual,
    /// `>`  - Greater than comparison
    Greater,
    /// `>=` - Greater than or equal comparison
    GreaterEqual,

    //===-----------------------------------------------------------------===//
    // Literals
    //===-----------------------------------------------------------------===//
    /// Identifier - `myapp`, `foo_bar`, `my-lib`
    Identifier,
    /// String literal - `"hello world"`, `"path/to/file"`
    String,
    /// Integer literal - `123`, `42`, `0`
    Number,
    /// Boolean literal - `true`
    True,
    /// Boolean literal - `false`
    False,

    //===-----------------------------------------------------------------===//
    // Special
    //===-----------------------------------------------------------------===//
    /// End of file marker
    EndOfFile,
}

impl TokenType {
    /// Returns true if this token type is a keyword (Project through False).
    #[inline(always)]
    pub fn is_keyword(self) -> bool {
        (self as u8) >= (TokenType::Project as u8) && (self as u8) <= (TokenType::False as u8)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// Represents a single lexical token
//
// Contains the token type and source location information for error reporting
// and debugging. The token's text can be extracted from source using Position + Length.
// Comments are attached via position references.
//
// Note: Leading and Trailing use 1-based positions (actual position + 1) so that
// position 0 can be used as a sentinel value meaning "no comment".
pub struct Token {
    /// Starting position in source text
    pub position: u32,
    /// Length of the token in the source text
    pub length: u32,
    /// Kind of the token
    pub kind: TokenType,
    /// 1-based position of comment(s) on line(s) above this token (0 = none)
    pub leading: u32,
    /// 1-based position of comment on same line after this token (0 = none)
    pub trailing: u32,
}
const _: () = assert!(size_of::<Token>() == 20);
