use std::borrow::Cow;

/// Severity level of a [`Diagnostic`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Severity {
    /// A hard error: analysis failed and the CLI should exit non-zero.
    #[default]
    Error,
    /// A warning: surfaced to the user but does not fail the build.
    Warning,
}

/// A diagnostic produced during compilation (lexing, parsing, or semantic analysis).
#[derive(Debug)]
pub struct Diagnostic {
    pub severity: Severity,
    pub message: Cow<'static, str>,
    pub position: u32,
    pub help: Cow<'static, str>,
}

impl Diagnostic {
    /// Create an error-severity diagnostic.
    pub fn new(
        message: impl Into<Cow<'static, str>>,
        position: u32,
        help: impl Into<Cow<'static, str>>,
    ) -> Self {
        Self {
            severity: Severity::Error,
            message: message.into(),
            position,
            help: help.into(),
        }
    }

    /// Create a warning-severity diagnostic.
    pub fn warning(
        message: impl Into<Cow<'static, str>>,
        position: u32,
        help: impl Into<Cow<'static, str>>,
    ) -> Self {
        Self {
            severity: Severity::Warning,
            message: message.into(),
            position,
            help: help.into(),
        }
    }
}
