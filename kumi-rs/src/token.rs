#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TokenType {
    // Top-Level Declarations
    Project,
    Workspace,
    Target,
    Dependencies,
    Options,
    Mixin,
    Profile,
    AtImport,
    Install,
    Package,
    Scripts,
    With,

    // Visibility Modifiers
    Public,
    Private,
    Interface,

    // Control Flow
    AtIf,
    AtElseIf,
    AtElse,
    AtFor,
    In,
    AtBreak,
    AtContinue,

    // Diagnostic Directives
    AtError,
    AtWarning,
    AtInfo,
    AtDebug,

    // Logical Operators
    And,
    Or,
    Not,

    // Operators and Punctuation
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    LeftParen,
    RightParen,

    Colon,
    Semicolon,
    Comma,

    Question,
    Dollar,
    Range,

    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,

    // Literals
    Identifier,
    String,
    Number,
    True,
    False,
    Comment,

    // Special
    EndOfFile,
}

#[derive(Debug, Clone, Copy)]
pub struct Token<'a> {
    pub value: &'a [u8],
    pub position: u32,
    pub ttype: TokenType,
}
