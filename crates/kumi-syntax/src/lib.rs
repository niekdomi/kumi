//! Kumi language frontend: tokens, lexer, AST, and parser for `.kumi` files.
//!
//! The AST borrows from the source buffer and is arena-allocated (see [`ast`]),
//! so the lexer, AST, and parser are lifetime-coupled and live in one crate.

pub mod ast;
pub mod lex;
pub mod parse;
