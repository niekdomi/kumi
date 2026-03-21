use kumi::lang::lex::{Lexer, Token, TokenType};

// Helper: lex input and return (tokens, errors). Tokens always include trailing EOF.
fn lex(input: &str) -> (Vec<Token>, Vec<kumi::diagnostics::Diagnostic>) {
    let lexer = Lexer::new(input.as_bytes());
    lexer.tokenize()
}

// Helper: lex input, assert no errors, return tokens.
fn lex_ok(input: &str) -> Vec<Token> {
    let (tokens, errors) = lex(input);
    assert!(errors.is_empty(), "unexpected errors: {:?}", errors);
    tokens
}

// Helper: lex input that should produce exactly one token + EOF, return that token.
fn lex_single(input: &str) -> Token {
    let tokens = lex_ok(input);
    assert_eq!(
        tokens.len(),
        2,
        "expected 1 token + EOF, got {}",
        tokens.len()
    );
    tokens[0]
}

// Helper: lex input and assert at least one error, return first error message.
fn lex_error(input: &str) -> kumi::diagnostics::Diagnostic {
    let (_, errors) = lex(input);
    assert!(!errors.is_empty(), "expected error but lexing succeeded");
    errors.into_iter().next().unwrap()
}

// Helper: extract token text from source.
fn text<'a>(source: &'a str, token: &Token) -> &'a str {
    &source[token.position as usize..(token.position + token.length) as usize]
}

//===---------------------------------------------------------------------===//
// Top-Level Declarations
//===---------------------------------------------------------------------===//

#[test]
fn top_level_keywords() {
    let cases: &[(&str, TokenType)] = &[
        ("project", TokenType::Project),
        ("workspace", TokenType::Workspace),
        ("target", TokenType::Target),
        ("dependencies", TokenType::Dependencies),
        ("options", TokenType::Options),
        ("mixin", TokenType::Mixin),
        ("profile", TokenType::Profile),
        ("@import", TokenType::AtImport),
        ("install", TokenType::Install),
        ("package", TokenType::Package),
        ("scripts", TokenType::Scripts),
        ("with", TokenType::With),
    ];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, expected, "input: {input}");
        assert_eq!(text(input, &token), input, "input: {input}");
    }
}

#[test]
fn top_level_project_declaration() {
    let input = "project myapp {\n    version: \"1.0.0\";\n}";
    let tokens = lex_ok(input);

    assert_eq!(tokens[0].ttype, TokenType::Project);
    assert_eq!(tokens[1].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[1]), "myapp");
    assert_eq!(tokens[2].ttype, TokenType::LeftBrace);

    assert_eq!(tokens[3].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[3]), "version");
    assert_eq!(tokens[4].ttype, TokenType::Colon);
    assert_eq!(tokens[5].ttype, TokenType::String);
    assert_eq!(text(input, &tokens[5]), "\"1.0.0\"");
    assert_eq!(tokens[6].ttype, TokenType::Semicolon);

    assert_eq!(tokens[7].ttype, TokenType::RightBrace);
    assert_eq!(tokens[8].ttype, TokenType::EndOfFile);
}

//===---------------------------------------------------------------------===//
// Visibility Modifiers
//===---------------------------------------------------------------------===//

#[test]
fn visibility_keywords() {
    let cases: &[(&str, TokenType)] = &[
        ("public", TokenType::Public),
        ("private", TokenType::Private),
        ("interface", TokenType::Interface),
    ];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, expected, "input: {input}");
        assert_eq!(text(input, &token), input, "input: {input}");
    }
}

//===---------------------------------------------------------------------===//
// Control Flow
//===---------------------------------------------------------------------===//

#[test]
fn control_flow_keywords() {
    let cases: &[(&str, TokenType)] = &[
        ("@if", TokenType::AtIf),
        ("@else-if", TokenType::AtElseIf),
        ("@else", TokenType::AtElse),
        ("@for", TokenType::AtFor),
        ("in", TokenType::In),
        ("@break", TokenType::AtBreak),
        ("@continue", TokenType::AtContinue),
    ];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, expected, "input: {input}");
        assert_eq!(text(input, &token), input, "input: {input}");
    }
}

#[test]
fn control_flow_conditional_with_function_call() {
    let input = "@if platform(windows) { }";
    let tokens = lex_ok(input);

    assert_eq!(tokens[0].ttype, TokenType::AtIf);
    assert_eq!(tokens[1].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[1]), "platform");
    assert_eq!(tokens[2].ttype, TokenType::LeftParen);
    assert_eq!(tokens[3].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[3]), "windows");
    assert_eq!(tokens[4].ttype, TokenType::RightParen);
    assert_eq!(tokens[5].ttype, TokenType::LeftBrace);
    assert_eq!(tokens[6].ttype, TokenType::RightBrace);
    assert_eq!(tokens[7].ttype, TokenType::EndOfFile);
}

#[test]
fn control_flow_for_loop_with_range() {
    let input = "@for worker in 0..7 { }";
    let tokens = lex_ok(input);

    assert_eq!(tokens[0].ttype, TokenType::AtFor);
    assert_eq!(tokens[1].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[1]), "worker");
    assert_eq!(tokens[2].ttype, TokenType::In);
    assert_eq!(tokens[3].ttype, TokenType::Number);
    assert_eq!(text(input, &tokens[3]), "0");
    assert_eq!(tokens[4].ttype, TokenType::Range);
    assert_eq!(text(input, &tokens[4]), "..");
    assert_eq!(tokens[5].ttype, TokenType::Number);
    assert_eq!(text(input, &tokens[5]), "7");
    assert_eq!(tokens[6].ttype, TokenType::LeftBrace);
    assert_eq!(tokens[7].ttype, TokenType::RightBrace);
    assert_eq!(tokens[8].ttype, TokenType::EndOfFile);
}

#[test]
fn control_flow_for_loop_with_list() {
    let input = "@for module in [core, renderer, audio] { }";
    let tokens = lex_ok(input);

    assert_eq!(tokens[0].ttype, TokenType::AtFor);
    assert_eq!(tokens[1].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[1]), "module");
    assert_eq!(tokens[2].ttype, TokenType::In);
    assert_eq!(tokens[3].ttype, TokenType::LeftBracket);
    assert_eq!(tokens[4].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[4]), "core");
    assert_eq!(tokens[5].ttype, TokenType::Comma);
    assert_eq!(tokens[6].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[6]), "renderer");
    assert_eq!(tokens[7].ttype, TokenType::Comma);
    assert_eq!(tokens[8].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[8]), "audio");
    assert_eq!(tokens[9].ttype, TokenType::RightBracket);
    assert_eq!(tokens[10].ttype, TokenType::LeftBrace);
    assert_eq!(tokens[11].ttype, TokenType::RightBrace);
    assert_eq!(tokens[12].ttype, TokenType::EndOfFile);
}

#[test]
fn control_flow_for_loop_with_function_call() {
    let input = "@for file in glob(\"*.cpp\") { }";
    let tokens = lex_ok(input);

    assert_eq!(tokens[0].ttype, TokenType::AtFor);
    assert_eq!(tokens[1].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[1]), "file");
    assert_eq!(tokens[2].ttype, TokenType::In);
    assert_eq!(tokens[3].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[3]), "glob");
    assert_eq!(tokens[4].ttype, TokenType::LeftParen);
    assert_eq!(tokens[5].ttype, TokenType::String);
    assert_eq!(text(input, &tokens[5]), "\"*.cpp\"");
    assert_eq!(tokens[6].ttype, TokenType::RightParen);
    assert_eq!(tokens[7].ttype, TokenType::LeftBrace);
    assert_eq!(tokens[8].ttype, TokenType::RightBrace);
    assert_eq!(tokens[9].ttype, TokenType::EndOfFile);
}

#[test]
fn control_flow_invalid_at_directives() {
    let cases = &[
        "@", "@invalid", "@123", "@-if", "@_else", "@IF", "@ELSE", "@For",
    ];
    for input in cases {
        let error = lex_error(input);
        assert!(
            error.message.contains("unexpected"),
            "input: {input}, message: {}",
            error.message
        );
    }
}

#[test]
fn control_flow_invalid_at_position_tracking() {
    let input = "project myapp @invalid";
    let error = lex_error(input);
    assert_eq!(error.position, 14);
    assert_eq!(input.as_bytes()[error.position as usize], b'@');
}

#[test]
fn control_flow_invalid_at_with_number() {
    let input = "@if test { } @123 { }";
    let error = lex_error(input);
    assert_eq!(error.position, 13);
    assert_eq!(input.as_bytes()[error.position as usize], b'@');
}

#[test]
fn control_flow_invalid_at_wrong_case() {
    let input = "target myapp { @IF }";
    let error = lex_error(input);
    assert_eq!(error.position, 15);
    assert_eq!(input.as_bytes()[error.position as usize], b'@');
}

//===---------------------------------------------------------------------===//
// Diagnostic Directives
//===---------------------------------------------------------------------===//

#[test]
fn diagnostic_directive_keywords() {
    let cases: &[(&str, TokenType)] = &[
        ("@error", TokenType::AtError),
        ("@warning", TokenType::AtWarning),
        ("@info", TokenType::AtInfo),
        ("@debug", TokenType::AtDebug),
    ];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, expected, "input: {input}");
        assert_eq!(text(input, &token), input, "input: {input}");
    }
}

//===---------------------------------------------------------------------===//
// Logical Operators
//===---------------------------------------------------------------------===//

#[test]
fn logical_operator_keywords() {
    let cases: &[(&str, TokenType)] = &[
        ("and", TokenType::And),
        ("or", TokenType::Or),
        ("not", TokenType::Not),
    ];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, expected, "input: {input}");
        assert_eq!(text(input, &token), input, "input: {input}");
    }
}

//===---------------------------------------------------------------------===//
// Operators and Punctuation
//===---------------------------------------------------------------------===//

#[test]
fn braces_brackets_parens() {
    let cases: &[(&str, TokenType)] = &[
        ("{", TokenType::LeftBrace),
        ("}", TokenType::RightBrace),
        ("[", TokenType::LeftBracket),
        ("]", TokenType::RightBracket),
        ("(", TokenType::LeftParen),
        (")", TokenType::RightParen),
    ];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, expected, "input: {input}");
        assert_eq!(text(input, &token), input, "input: {input}");
    }
}

#[test]
fn delimiters() {
    let cases: &[(&str, TokenType)] = &[
        (":", TokenType::Colon),
        (";", TokenType::Semicolon),
        (",", TokenType::Comma),
    ];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, expected, "input: {input}");
        assert_eq!(text(input, &token), input, "input: {input}");
    }
}

#[test]
fn delimiter_property_assignment_sequence() {
    let input = "sources: \"*.cpp\";";
    let tokens = lex_ok(input);

    assert_eq!(tokens[0].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[0]), "sources");
    assert_eq!(tokens[1].ttype, TokenType::Colon);
    assert_eq!(tokens[2].ttype, TokenType::String);
    assert_eq!(text(input, &tokens[2]), "\"*.cpp\"");
    assert_eq!(tokens[3].ttype, TokenType::Semicolon);
    assert_eq!(tokens[4].ttype, TokenType::EndOfFile);
}

#[test]
fn delimiter_comma_separated_list() {
    let input = "authors: \"Alice\", \"Bob\", \"Charlie\";";
    let tokens = lex_ok(input);

    assert_eq!(tokens[0].ttype, TokenType::Identifier);
    assert_eq!(text(input, &tokens[0]), "authors");
    assert_eq!(tokens[1].ttype, TokenType::Colon);
    assert_eq!(tokens[2].ttype, TokenType::String);
    assert_eq!(text(input, &tokens[2]), "\"Alice\"");
    assert_eq!(tokens[3].ttype, TokenType::Comma);
    assert_eq!(tokens[4].ttype, TokenType::String);
    assert_eq!(text(input, &tokens[4]), "\"Bob\"");
    assert_eq!(tokens[5].ttype, TokenType::Comma);
    assert_eq!(tokens[6].ttype, TokenType::String);
    assert_eq!(text(input, &tokens[6]), "\"Charlie\"");
    assert_eq!(tokens[7].ttype, TokenType::Semicolon);
    assert_eq!(tokens[8].ttype, TokenType::EndOfFile);
}

#[test]
fn special_operators() {
    let cases: &[(&str, TokenType)] = &[
        ("?", TokenType::Question),
        ("$", TokenType::Dollar),
        ("..", TokenType::Range),
    ];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, expected, "input: {input}");
        assert_eq!(text(input, &token), input, "input: {input}");
    }
}

#[test]
fn special_operator_range_expression() {
    let input = "0..10";
    let tokens = lex_ok(input);

    assert_eq!(tokens[0].ttype, TokenType::Number);
    assert_eq!(text(input, &tokens[0]), "0");
    assert_eq!(tokens[1].ttype, TokenType::Range);
    assert_eq!(text(input, &tokens[1]), "..");
    assert_eq!(tokens[2].ttype, TokenType::Number);
    assert_eq!(text(input, &tokens[2]), "10");
    assert_eq!(tokens[3].ttype, TokenType::EndOfFile);
}

#[test]
fn special_operator_invalid_single_dot() {
    let input = "x . y";
    let error = lex_error(input);
    assert_eq!(error.position, 2);
    assert_eq!(input.as_bytes()[error.position as usize], b'.');
    assert!(error.message.contains("unexpected"));
}

#[test]
fn comparison_operators() {
    let cases: &[(&str, TokenType)] = &[
        ("==", TokenType::Equal),
        ("!=", TokenType::NotEqual),
        ("<", TokenType::Less),
        ("<=", TokenType::LessEqual),
        (">", TokenType::Greater),
        (">=", TokenType::GreaterEqual),
    ];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, expected, "input: {input}");
        assert_eq!(text(input, &token), input, "input: {input}");
    }
}

#[test]
fn comparison_operators_adjacent_without_whitespace() {
    let input = "!=>=<===";
    let tokens = lex_ok(input);

    assert_eq!(tokens[0].ttype, TokenType::NotEqual);
    assert_eq!(tokens[1].ttype, TokenType::GreaterEqual);
    assert_eq!(tokens[2].ttype, TokenType::LessEqual);
    assert_eq!(tokens[3].ttype, TokenType::Equal);
    assert_eq!(tokens[4].ttype, TokenType::EndOfFile);
}

#[test]
fn comparison_operator_invalid_single_equals() {
    let input = "x = 5";
    let error = lex_error(input);
    assert_eq!(error.position, 2);
    assert_eq!(input.as_bytes()[error.position as usize], b'=');
    assert!(error.message.contains("unexpected"));
}

#[test]
fn comparison_operator_invalid_single_exclamation() {
    let input = "x ! y";
    let error = lex_error(input);
    assert_eq!(error.position, 2);
    assert_eq!(input.as_bytes()[error.position as usize], b'!');
    assert!(error.message.contains("unexpected"));
}

#[test]
fn comparison_operator_invalid_comment_start() {
    let input = "test / 5";
    let error = lex_error(input);
    assert_eq!(error.position, 5);
    assert_eq!(input.as_bytes()[error.position as usize], b'/');
    assert!(error.message.contains("unexpected"));
}

#[test]
fn position_tracking_in_token_sequence() {
    let tokens = lex_ok("target myapp {");
    assert_eq!(tokens[0].position, 0);
    assert_eq!(tokens[1].position, 7);
    assert_eq!(tokens[2].position, 13);
}

//===---------------------------------------------------------------------===//
// Literals
//===---------------------------------------------------------------------===//

#[test]
fn identifier_valid() {
    let cases = &[
        "myvar",
        "my_var",
        "my-var",
        "var123",
        "_private",
        "camelCase",
        "PascalCase",
        "snake_case",
        "SCREAMING_CASE",
        "a",
        "a1",
        "a-b-c",
        "a_b_c",
    ];

    for input in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, TokenType::Identifier, "input: {input}");
        assert_eq!(text(input, &token), *input, "input: {input}");
    }
}

#[test]
fn identifier_keyword_like() {
    let cases = &["projects", "targeting", "ands", "trueish", "falsey"];

    for input in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, TokenType::Identifier, "input: {input}");
        assert_eq!(text(input, &token), *input, "input: {input}");
    }
}

#[test]
fn identifier_invalid_starts() {
    let cases = &["*", "*/", "#", "~", "%", "&", "^", "|", "\\", "`"];

    for input in cases {
        let error = lex_error(input);
        assert!(
            error.message.contains("unexpected"),
            "input: {input}, message: {}",
            error.message
        );
    }
}

#[test]
fn string_simple() {
    let cases: &[(&str, &str)] = &[
        ("\"\"", "\"\""),
        ("\"hello\"", "\"hello\""),
        ("\"hello world\"", "\"hello world\""),
        ("\"123\"", "\"123\""),
    ];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, TokenType::String, "input: {input}");
        assert_eq!(text(input, &token), expected, "input: {input}");
    }
}

#[test]
fn string_escape_sequences() {
    let cases: &[(&str, &str)] = &[
        ("\"hello\\nworld\"", "\"hello\\nworld\""),
        ("\"hello\\tworld\"", "\"hello\\tworld\""),
        ("\"hello\\rworld\"", "\"hello\\rworld\""),
        ("\"say \\\"hello\\\"\"", "\"say \\\"hello\\\"\""),
        ("\"path\\\\to\\\\file\"", "\"path\\\\to\\\\file\""),
    ];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, TokenType::String, "input: {input}");
        assert_eq!(text(input, &token), expected, "input: {input}");
    }
}

#[test]
fn string_unterminated_eof() {
    let error = lex_error("\"unterminated");
    assert!(error.message.contains("unterminated"));
}

#[test]
fn string_unterminated_newline() {
    let error = lex_error("\"line1\nline2\"");
    assert!(error.message.contains("unterminated"));
}

#[test]
fn string_invalid_escape_sequences() {
    let cases = &[
        "\"invalid\\x\"",
        "\"invalid\\a\"",
        "\"invalid\\b\"",
        "\"invalid\\f\"",
        "\"invalid\\v\"",
        "\"invalid\\0\"",
        "\"invalid\\1\"",
    ];

    for input in cases {
        let error = lex_error(input);
        assert!(
            error.message.contains("invalid escape"),
            "input: {input}, message: {}",
            error.message
        );
    }
}

#[test]
fn number_literals() {
    let cases = &["0", "1", "42", "123", "999999", "1234567890"];

    for input in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, TokenType::Number, "input: {input}");
        assert_eq!(text(input, &token), *input, "input: {input}");
    }
}

#[test]
fn boolean_literals() {
    let cases: &[(&str, TokenType)] = &[("true", TokenType::True), ("false", TokenType::False)];

    for &(input, expected) in cases {
        let token = lex_single(input);
        assert_eq!(token.ttype, expected, "input: {input}");
        assert_eq!(text(input, &token), input, "input: {input}");
    }
}

//===---------------------------------------------------------------------===//
// Comments
//===---------------------------------------------------------------------===//

#[test]
fn comment_line_skipped() {
    // Line comments are not emitted as tokens, just skipped
    let cases = &["//", "// comment", "// hello world", "//no space", "// /*"];

    for input in cases {
        let tokens = lex_ok(input);
        assert_eq!(tokens.len(), 1, "input: {input}");
        assert_eq!(tokens[0].ttype, TokenType::EndOfFile, "input: {input}");
    }
}

#[test]
fn comment_block_skipped() {
    let cases = &[
        "/**/",
        "/* comment */",
        "/* multi\nline */",
        "/* /* multiple /* */",
    ];

    for input in cases {
        let tokens = lex_ok(input);
        assert_eq!(tokens.len(), 1, "input: {input}");
        assert_eq!(tokens[0].ttype, TokenType::EndOfFile, "input: {input}");
    }
}

#[test]
fn comment_block_unterminated() {
    let error = lex_error("/* unterminated");
    assert!(error.message.contains("unterminated"));
}

#[test]
fn comment_block_unterminated_multiline() {
    let input = "project /* unterminated\nstring";
    let error = lex_error(input);
    assert!(error.message.contains("unterminated"));
}

#[test]
fn comment_leading_attached_to_next_token() {
    let input = "// leading\nproject";
    let tokens = lex_ok(input);
    assert_eq!(tokens[0].ttype, TokenType::Project);
    assert_ne!(tokens[0].leading, 0, "leading comment should be attached");
}

#[test]
fn comment_trailing_attached_to_previous_token() {
    let input = "project // trailing";
    let tokens = lex_ok(input);
    assert_eq!(tokens[0].ttype, TokenType::Project);
    assert_ne!(tokens[0].trailing, 0, "trailing comment should be attached");
}

//===---------------------------------------------------------------------===//
// End of File
//===---------------------------------------------------------------------===//

#[test]
fn eof_empty_input() {
    let tokens = lex_ok("");
    assert_eq!(tokens.len(), 1);
    assert_eq!(tokens[0].ttype, TokenType::EndOfFile);
    assert_eq!(tokens[0].position, 0);
}

#[test]
fn eof_whitespace_only() {
    let tokens = lex_ok("   \n\t  ");
    assert_eq!(tokens.len(), 1);
    assert_eq!(tokens[0].ttype, TokenType::EndOfFile);
}

#[test]
fn eof_after_tokens() {
    let tokens = lex_ok("project myapp");
    assert_eq!(tokens.len(), 3);
    assert_eq!(tokens[0].position, 0);
    assert_eq!(tokens[1].position, 8);
    assert_eq!(tokens[2].ttype, TokenType::EndOfFile);
}

#[test]
fn eof_with_leading_whitespace() {
    let tokens = lex_ok("  target");
    assert_eq!(tokens[0].position, 2);
}

#[test]
fn eof_with_newlines() {
    let tokens = lex_ok("target\nproject");
    assert_eq!(tokens[0].position, 0);
    assert_eq!(tokens[1].position, 7);
}
