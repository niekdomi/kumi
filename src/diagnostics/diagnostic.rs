use std::borrow::Cow;

/// A diagnostic produced during compilation (lexing, parsing, or semantic analysis).
#[derive(Debug)]
pub struct Diagnostic {
    pub message: Cow<'static, str>,
    pub position: u32,
    pub help: Cow<'static, str>,
}

impl Diagnostic {
    pub fn new(
        message: impl Into<Cow<'static, str>>,
        position: u32,
        help: impl Into<Cow<'static, str>>,
    ) -> Self {
        Self {
            message: message.into(),
            position,
            help: help.into(),
        }
    }
}
