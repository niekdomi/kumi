use crate::diagnostics::Diagnostic;
use crate::lang::lex::char_utils::{is_digit, is_identifier, is_space};
use crate::lang::lex::token::{Token, TokenType};
use memchr::{memchr, memmem};

/// Lexical analyzer that converts a byte-slice input into a stream of Tokens.
///
/// The lexer is a single-pass, position-based tokenizer. It supports
/// single-character tokens, multi-character operators, numbers, strings,
/// identifiers/keywords, and both line and block comments.
///
/// Note: comments are used to set leading/trailing comment positions on
/// tokens rather than being emitted as separate tokens by this lexer.
pub struct Lexer<'a> {
    /// Input buffer being tokenized
    input: &'a [u8],
    /// Current byte offset into `input`
    position: u32,
    /// Vector containing the tokens emitted so far
    tokens: Vec<Token>,
    /// Position of the first leading comment before the next token (None if no pending leading comment)
    first_leading_comment_pos: Option<u32>,
}

impl<'lex_impl> Lexer<'lex_impl> {
    /// Constructs a lexer for the given input buffer.
    pub const fn new(input: &'lex_impl [u8]) -> Self {
        Self {
            input,
            position: 0,
            tokens: Vec::new(),
            first_leading_comment_pos: None,
        }
    }

    /// Tokenizes the entire input and returns a vector of tokens on success,
    /// or a Diagnostic on failure.
    ///
    /// The returned vector contains tokens in the order they appear in the input.
    /// The `EndOfFile` token is always emitted as the last token.
    pub fn tokenize(mut self) -> (Vec<Token>, Vec<Diagnostic>) {
        self.tokens.reserve(self.input.len());
        let mut errors = Vec::new();

        loop {
            let pre_pos = self.position;
            match self.next_token() {
                Ok(None) => continue,
                Ok(Some(token)) => {
                    self.tokens.push(token);

                    if token.kind == TokenType::EndOfFile {
                        break;
                    }
                }
                Err(err) => {
                    errors.push(err);
                    // Force advance if we didn't consume anything to prevent infinite loop
                    if self.position == pre_pos && !self.at_end() {
                        self.advance();
                    }
                }
            }
        }

        (self.tokens, errors)
    }

    /// Advance the current position by one byte.
    #[inline(always)]
    const fn advance(&mut self) {
        self.position += 1;
    }

    /// Returns true if the lexer has reached the end of the input.
    #[inline(always)]
    const fn at_end(&self) -> bool {
        self.position as usize >= self.input.len()
    }

    /// If the remaining input starts with `s`, consume it and return true.
    #[inline(always)]
    fn match_string(&mut self, s: &[u8]) -> bool {
        if self.input[self.position as usize..].starts_with(s) {
            self.position += s.len() as u32;
            true
        } else {
            false
        }
    }

    /// Peek the current byte without consuming it. Returns 0 at EOF.
    #[inline(always)]
    fn peek(&self) -> u8 {
        if self.position as usize >= self.input.len() {
            0
        } else {
            self.input[self.position as usize]
        }
    }

    /// Skip any whitespace characters (space, tab, newline, ...).
    #[inline(always)]
    fn skip_whitespace(&mut self) {
        while !self.at_end() && is_space(self.peek()) {
            self.position += 1;
        }
    }

    /// Read and return the next token from the input. If a lexical error is
    /// encountered, a Diagnostic is returned.
    ///
    /// The lexer treats comments specially: comments are not emitted as tokens
    /// by this function but are used to set `leading`/`trailing` fields on
    /// neighboring tokens. Leading comments are attached to the next emitted
    /// (non-comment) token and trailing comments are attached to the previous
    /// emitted token if they appear on the same line as the end of that token.
    #[inline(always)]
    fn next_token(&mut self) -> Result<Option<Token>, Diagnostic> {
        self.skip_whitespace();

        if self.at_end() {
            let mut token = Token {
                position: self.position,
                length: 0,
                kind: TokenType::EndOfFile,
                leading: 0,
                trailing: 0,
            };
            if let Some(pos) = self.first_leading_comment_pos.take() {
                token.leading = pos + 1;
            }
            return Ok(Some(token));
        }

        let mut token = match self.peek() {
            b'{' => self.lex_single_char(TokenType::LeftBrace),
            b'}' => self.lex_single_char(TokenType::RightBrace),
            b'[' => self.lex_single_char(TokenType::LeftBracket),
            b']' => self.lex_single_char(TokenType::RightBracket),
            b'(' => self.lex_single_char(TokenType::LeftParen),
            b')' => self.lex_single_char(TokenType::RightParen),
            b':' => self.lex_single_char(TokenType::Colon),
            b';' => self.lex_single_char(TokenType::Semicolon),
            b',' => self.lex_single_char(TokenType::Comma),
            b'?' => self.lex_single_char(TokenType::Question),
            b'$' => self.lex_single_char(TokenType::Dollar),
            b'.' => self.lex_dot()?,
            b'!' => self.lex_bang()?,
            b'=' => self.lex_equal()?,
            b'<' => self.lex_less(),
            b'>' => self.lex_greater(),
            b'"' => self.lex_string()?,
            b'@' => self.lex_at()?,
            b'/' => {
                self.lex_comment()?;
                return Ok(None);
            }
            b'0'..=b'9' => self.lex_number(),
            _ => self.lex_identifier_or_keyword()?,
        };

        if let Some(pos) = self.first_leading_comment_pos.take() {
            token.leading = pos + 1;
        }

        Ok(Some(token))
    }

    //===------------------------------------------------------------------===//
    // Lexing Helpers
    //===------------------------------------------------------------------===//

    #[inline(always)]
    fn lex_at(&mut self) -> Result<Token, Diagnostic> {
        static KEYWORDS: &[(&[u8], TokenType)] = &[
            (b"@else-if", TokenType::AtElseIf),
            (b"@else", TokenType::AtElse),
            (b"@if", TokenType::AtIf),
            (b"@for", TokenType::AtFor),
            (b"@break", TokenType::AtBreak),
            (b"@continue", TokenType::AtContinue),
            (b"@error", TokenType::AtError),
            (b"@warning", TokenType::AtWarning),
            (b"@info", TokenType::AtInfo),
            (b"@debug", TokenType::AtDebug),
        ];

        let start_pos = self.position;

        for &(keyword, tt) in KEYWORDS {
            if self.match_string(keyword) {
                return Ok(Token {
                    position: start_pos,
                    length: self.position - start_pos,
                    kind: tt,
                    leading: 0,
                    trailing: 0,
                });
            }
        }

        Err(Diagnostic::new("unexpected character after '@'", start_pos, ""))
    }

    #[inline(always)]
    fn lex_bang(&mut self) -> Result<Token, Diagnostic> {
        let start_pos = self.position;

        if self.match_string(b"!=") {
            return Ok(Token {
                position: start_pos,
                length: self.position - start_pos,
                kind: TokenType::NotEqual,
                leading: 0,
                trailing: 0,
            });
        }

        Err(Diagnostic::new("unexpected character after '!'", start_pos, ""))
    }

    #[inline(always)]
    fn lex_comment(&mut self) -> Result<(), Diagnostic> {
        let start_pos = self.position;

        let is_block = if self.match_string(b"//") {
            false
        } else if self.match_string(b"/*") {
            true
        } else {
            return Err(Diagnostic::new("unexpected character after '/'", start_pos, ""));
        };

        if is_block {
            let rem = &self.input[self.position as usize..];
            if let Some(idx) = memmem::find(rem, b"*/") {
                self.position += (idx + 2) as u32
            } else {
                self.position = self.input.len() as u32;
                return Err(Diagnostic::new("unterminated block comment", start_pos, ""));
            }
        } else {
            let rem = &self.input[self.position as usize..];
            match memchr(b'\n', rem) {
                Some(idx) => self.position += idx as u32,
                None => self.position = self.input.len() as u32,
            }
        }

        // Classify and attach
        if let Some(last) = self.tokens.last_mut() {
            let between = &self.input[(last.position + last.length) as usize..start_pos as usize];
            if between.contains(&b'\n') {
                self.first_leading_comment_pos.get_or_insert(start_pos);
            } else {
                last.trailing = start_pos + 1;
            }
        } else {
            self.first_leading_comment_pos.get_or_insert(start_pos);
        }

        Ok(())
    }

    #[inline(always)]
    fn lex_dot(&mut self) -> Result<Token, Diagnostic> {
        let start_pos = self.position;

        if self.match_string(b"..") {
            return Ok(Token {
                position: start_pos,
                length: self.position - start_pos,
                kind: TokenType::Range,
                leading: 0,
                trailing: 0,
            });
        }

        Err(Diagnostic::new("unexpected character after '.'", start_pos, ""))
    }

    #[inline(always)]
    fn lex_equal(&mut self) -> Result<Token, Diagnostic> {
        let start_pos = self.position;

        if self.match_string(b"==") {
            return Ok(Token {
                position: start_pos,
                length: self.position - start_pos,
                kind: TokenType::Equal,
                leading: 0,
                trailing: 0,
            });
        }

        Err(Diagnostic::new("unexpected character after '='", start_pos, ""))
    }

    #[inline(always)]
    fn lex_greater(&mut self) -> Token {
        let start_pos = self.position;

        if self.match_string(b">=") {
            return Token {
                position: start_pos,
                length: self.position - start_pos,
                kind: TokenType::GreaterEqual,
                leading: 0,
                trailing: 0,
            };
        }

        self.advance();
        Token {
            position: start_pos,
            length: self.position - start_pos,
            kind: TokenType::Greater,
            leading: 0,
            trailing: 0,
        }
    }

    #[inline(always)]
    fn lex_less(&mut self) -> Token {
        let start_pos = self.position;

        if self.match_string(b"<=") {
            return Token {
                position: start_pos,
                length: self.position - start_pos,
                kind: TokenType::LessEqual,
                leading: 0,
                trailing: 0,
            };
        }

        self.advance();
        Token {
            position: start_pos,
            length: self.position - start_pos,
            kind: TokenType::Less,
            leading: 0,
            trailing: 0,
        }
    }

    #[inline(always)]
    fn lex_number(&mut self) -> Token {
        let start_pos = self.position;

        while !self.at_end() && is_digit(self.peek()) {
            self.advance();
        }

        Token {
            position: start_pos,
            length: self.position - start_pos,
            kind: TokenType::Number,
            leading: 0,
            trailing: 0,
        }
    }

    #[inline(always)]
    const fn lex_single_char(&mut self, kind: TokenType) -> Token {
        let position = self.position;
        self.advance();
        Token {
            position,
            length: 1,
            kind,
            leading: 0,
            trailing: 0,
        }
    }

    #[inline(always)]
    fn lex_string(&mut self) -> Result<Token, Diagnostic> {
        let start_pos = self.position;
        self.advance();

        while self.peek() != b'"' {
            if self.at_end() {
                return Err(Diagnostic::new("unterminated string literal", start_pos, ""));
            }

            match self.peek() {
                b'\n' | b'\r' => {
                    return Err(Diagnostic::new("unterminated string literal", start_pos, ""));
                }
                b'\\' => {
                    self.advance(); // Consume '\'
                    let next = self.peek();
                    if next != b'"' && next != b'n' && next != b't' && next != b'r' && next != b'\\'
                    {
                        let err_pos = self.position - 1;
                        // Recover: consume rest of valid-looking string to resume parsing later
                        while !self.at_end() && self.peek() != b'"' && self.peek() != b'\n' {
                            self.advance();
                        }
                        if self.peek() == b'"' {
                            self.advance();
                        }
                        return Err(Diagnostic::new(
                            "invalid escape sequence",
                            err_pos,
                            "valid escapes: \\\", \\n, \\t, \\r, \\\\",
                        ));
                    }
                    self.advance(); // Consume escaped character
                }
                _ => {
                    self.advance(); // Consume regular character
                }
            }
        }

        self.advance(); // Consume closing '"'

        Ok(Token {
            position: start_pos,
            length: self.position - start_pos,
            kind: TokenType::String,
            leading: 0,
            trailing: 0,
        })
    }

    #[inline(always)]
    fn lex_identifier_or_keyword(&mut self) -> Result<Token, Diagnostic> {
        let start_pos = self.position;
        let input = self.input;

        while !self.at_end() && is_identifier(self.peek()) {
            self.position += 1;
        }

        let text = &input[start_pos as usize..self.position as usize];

        if text.is_empty() {
            return Err(Diagnostic::new(
                "unexpected character",
                self.position,
                "expected an identifier, keyword, or other valid token",
            ));
        }

        static KEYWORDS: &[(&[u8], TokenType)] = &[
            // Top-Level Declarations
            (b"project", TokenType::Project),
            (b"workspace", TokenType::Workspace),
            (b"target", TokenType::Target),
            (b"dependencies", TokenType::Dependencies),
            (b"options", TokenType::Options),
            (b"mixin", TokenType::Mixin),
            (b"profile", TokenType::Profile),
            (b"install", TokenType::Install),
            (b"package", TokenType::Package),
            (b"script", TokenType::Script),
            (b"with", TokenType::With),
            // Visibility Modifiers
            (b"public", TokenType::Public),
            (b"private", TokenType::Private),
            (b"interface", TokenType::Interface),
            // Control Flow
            (b"in", TokenType::In),
            // Logical Operators
            (b"and", TokenType::And),
            (b"or", TokenType::Or),
            (b"not", TokenType::Not),
            // Literals
            (b"true", TokenType::True),
            (b"false", TokenType::False),
        ];

        for &(keyword, tt) in KEYWORDS {
            if text == keyword {
                return Ok(Token {
                    position: start_pos,
                    length: self.position - start_pos,
                    kind: tt,
                    leading: 0,
                    trailing: 0,
                });
            }
        }

        Ok(Token {
            position: start_pos,
            length: self.position - start_pos,
            kind: TokenType::Identifier,
            leading: 0,
            trailing: 0,
        })
    }
}
