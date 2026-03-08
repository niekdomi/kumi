use crate::char_utils::*;
pub use crate::token::*;

pub struct ParseError {
    pub message: &'static str,
    pub position: u32,
    pub label: &'static str,
    pub help: &'static str,
}

pub struct Lexer<'a> {
    input: &'a [u8],
    position: u32,
}

impl<'a> Lexer<'a> {
    pub fn new(input: &'a [u8]) -> Self {
        Self { input, position: 0 }
    }

    pub fn tokenize(&mut self) -> Result<Vec<Token<'a>>, ParseError> {
        let mut tokens = Vec::with_capacity(self.input.len());

        loop {
            let token = self.next_token()?;
            tokens.push(token);

            if token.ttype == TokenType::EndOfFile {
                break;
            }
        }

        Ok(tokens)
    }

    #[inline(always)]
    fn advance(&mut self) {
        self.position += 1;
    }

    #[inline(always)]
    fn at_end(&mut self) -> bool {
        self.position as usize >= self.input.len()
    }

    #[inline(always)]
    fn match_string(&mut self, s: &[u8]) -> bool {
        if self.input[self.position as usize..].starts_with(s) {
            self.position += s.len() as u32;
            true
        } else {
            false
        }
    }

    #[inline(always)]
    fn peek(&self) -> u8 {
        self.input[self.position as usize]
    }

    #[inline(always)]
    fn skip_whitespace(&mut self) {
        while !self.at_end() && is_space(self.peek()) {
            self.position += 1;
        }
    }

    #[inline(always)]
    fn next_token(&mut self) -> Result<Token<'a>, ParseError> {
        self.skip_whitespace();

        if self.at_end() {
            return Ok(Token {
                value: b"",
                position: self.position,
                ttype: TokenType::EndOfFile,
            });
        }

        return match self.peek() {
            b'{' => self.lex_single_char(TokenType::LeftBrace, b"{"),
            b'}' => self.lex_single_char(TokenType::RightBrace, b"}"),
            b'[' => self.lex_single_char(TokenType::LeftBracket, b"["),
            b']' => self.lex_single_char(TokenType::RightBracket, b"]"),
            b'(' => self.lex_single_char(TokenType::LeftParen, b"("),
            b')' => self.lex_single_char(TokenType::RightParen, b")"),
            b':' => self.lex_single_char(TokenType::Colon, b":"),
            b';' => self.lex_single_char(TokenType::Semicolon, b";"),
            b',' => self.lex_single_char(TokenType::Comma, b","),
            b'?' => self.lex_single_char(TokenType::Question, b"?"),
            b'$' => self.lex_single_char(TokenType::Dollar, b"$"),
            b'.' => self.lex_dot(),
            b'!' => self.lex_bang(),
            b'=' => self.lex_equal(),
            b'<' => self.lex_less(),
            b'>' => self.lex_greater(),
            b'"' => self.lex_string(),
            b'@' => self.lex_at(),
            b'/' => self.lex_comment(),
            b'0'..=b'9' => self.lex_number(),
            _ => self.lex_identifier_or_keyword(),
        };
    }

    //===------------------------------------------------------------------===//
    // Lexing Helpers
    //===------------------------------------------------------------------===//

    #[inline(always)]
    fn lex_at(&mut self) -> Result<Token<'a>, ParseError> {
        static KEYWORDS: &[(&[u8], TokenType)] = &[
            (b"@import", TokenType::AtImport),
            (b"@if", TokenType::AtIf),
            (b"@else-if", TokenType::AtElseIf),
            (b"@else", TokenType::AtElse),
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
                    value: keyword,
                    position: start_pos,
                    ttype: tt,
                });
            }
        }

        Err(ParseError {
            message: "unexpected character after '@'",
            position: start_pos,
            label: "",
            help: "",
        })
    }

    #[inline(always)]
    fn lex_bang(&mut self) -> Result<Token<'a>, ParseError> {
        let start_pos = self.position;

        if self.match_string(b"!=") {
            return Ok(Token {
                value: b"!=",
                position: start_pos,
                ttype: TokenType::NotEqual,
            });
        }

        Err(ParseError {
            message: "unexpected character after '!'",
            position: start_pos,
            label: "expected '='",
            help: "",
        })
    }

    #[inline(always)]
    fn lex_comment(&mut self) -> Result<Token<'a>, ParseError> {
        let start_pos = self.position;

        if self.match_string(b"//") {
            let mut pos = self.position as usize;
            let input = self.input;
            while pos < input.len() && input[pos] != b'\n' {
                pos += 1;
            }
            self.position = pos as u32;
            return Ok(Token {
                value: &input[start_pos as usize..pos],
                position: start_pos,
                ttype: TokenType::Comment,
            });
        }

        if self.match_string(b"/*") {
            let mut pos = self.position as usize;
            let input = self.input;
            let mut found = false;
            while pos + 1 < input.len() {
                if input[pos] == b'*' && input[pos + 1] == b'/' {
                    found = true;
                    pos += 2;
                    break;
                }
                pos += 1;
            }
            if found {
                self.position = pos as u32;
                return Ok(Token {
                    value: &input[start_pos as usize..pos],
                    position: start_pos,
                    ttype: TokenType::Comment,
                });
            }
            self.position = input.len() as u32;
            return Err(ParseError {
                message: "unterminated block comment",
                position: self.position,
                label: "missing closing */",
                help: "",
            });
        }

        Err(ParseError {
            message: "unexpected character after '/'",
            position: self.position,
            label: "expected '/' or '*'",
            help: "",
        })
    }

    #[inline(always)]
    fn lex_dot(&mut self) -> Result<Token<'a>, ParseError> {
        let start_pos = self.position;

        if self.match_string(b"..") {
            return Ok(Token {
                value: b"..",
                position: start_pos,
                ttype: TokenType::Range,
            });
        }

        Err(ParseError {
            message: "unexpected character after '.'",
            position: start_pos,
            label: "expected '.'",
            help: "",
        })
    }

    #[inline(always)]
    fn lex_equal(&mut self) -> Result<Token<'a>, ParseError> {
        let start_pos = self.position;

        if self.match_string(b"==") {
            return Ok(Token {
                value: b"==",
                position: start_pos,
                ttype: TokenType::Equal,
            });
        }

        Err(ParseError {
            message: "unexpected character after '='",
            position: start_pos,
            label: "expected '='",
            help: "",
        })
    }

    #[inline(always)]
    fn lex_greater(&mut self) -> Result<Token<'a>, ParseError> {
        let start_pos = self.position;

        if self.match_string(b">=") {
            return Ok(Token {
                value: b">=",
                position: start_pos,
                ttype: TokenType::GreaterEqual,
            });
        }

        self.advance();
        Ok(Token {
            value: b">",
            position: start_pos,
            ttype: TokenType::Greater,
        })
    }

    #[inline(always)]
    fn lex_less(&mut self) -> Result<Token<'a>, ParseError> {
        let start_pos = self.position;

        if self.match_string(b"<=") {
            return Ok(Token {
                value: b"<=",
                position: start_pos,
                ttype: TokenType::LessEqual,
            });
        }

        self.advance();
        Ok(Token {
            value: b"<",
            position: start_pos,
            ttype: TokenType::Less,
        })
    }

    #[inline(always)]
    fn lex_number(&mut self) -> Result<Token<'a>, ParseError> {
        let start_pos = self.position;

        while !self.at_end() && is_digit(self.input[self.position as usize]) {
            self.position += 1;
        }

        Ok(Token {
            value: &self.input[start_pos as usize..self.position as usize],
            position: start_pos,
            ttype: TokenType::Number,
        })
    }

    #[inline(always)]
    fn lex_single_char(
        &mut self,
        ttype: TokenType,
        value: &'static [u8],
    ) -> Result<Token<'a>, ParseError> {
        let position = self.position;
        self.position += 1;
        Ok(Token {
            value,
            position,
            ttype,
        })
    }

    #[inline(always)]
    fn lex_string(&mut self) -> Result<Token<'a>, ParseError> {
        let start_pos = self.position;
        let mut pos = (start_pos + 1) as usize;
        let input = self.input;

        while pos < input.len() {
            let ch = input[pos];
            if ch == b'"' {
                pos += 1;
                self.position = pos as u32;
                return Ok(Token {
                    value: &input[start_pos as usize..pos],
                    position: start_pos,
                    ttype: TokenType::String,
                });
            }
            if ch == b'\n' || ch == b'\r' {
                self.position = pos as u32;
                return Err(ParseError {
                    message: "unterminated string literal",
                    position: start_pos,
                    label: "missing closing \"",
                    help: "strings cannot span multiple lines",
                });
            }
            if ch == b'\\' {
                pos += 1;
                if pos >= input.len() {
                    break;
                }
                let next = input[pos];
                if next != b'"' && next != b'n' && next != b't' && next != b'r' && next != b'\\' {
                    self.position = pos as u32;
                    return Err(ParseError {
                        message: "invalid escape sequence",
                        position: (pos - 1) as u32,
                        label: "unknown escape character",
                        help: "valid escapes: \\\", \\n, \\t, \\r, \\\\",
                    });
                }
                pos += 1;
            } else {
                pos += 1;
            }
        }
        self.position = pos as u32;
        Err(ParseError {
            message: "unterminated string literal",
            position: start_pos,
            label: "missing closing \"",
            help: "",
        })
    }

    #[inline(always)]
    fn lex_identifier_or_keyword(&mut self) -> Result<Token<'a>, ParseError> {
        let start_pos = self.position;
        let input = self.input;

        while !self.at_end() && is_identifier(input[self.position as usize]) {
            self.position += 1;
        }

        let text = &input[start_pos as usize..self.position as usize];

        if text.is_empty() {
            return Err(ParseError {
                message: "unexpected character",
                position: self.position,
                label: "invalid character here",
                help: "expected an identifier, keyword, or other valid token",
            });
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
            (b"scripts", TokenType::Scripts),
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
                    value: text,
                    position: start_pos,
                    ttype: tt,
                });
            }
        }

        Ok(Token {
            value: text,
            position: start_pos,
            ttype: TokenType::Identifier,
        })
    }
}
