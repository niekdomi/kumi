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

/// A secondary, related source location attached to a [`Diagnostic`] — e.g. the
/// first definition when reporting a duplicate. Rendered as an extra underline.
#[derive(Debug)]
pub struct SecondaryLabel {
    pub start: u32,
    pub end: u32,
    pub message: Cow<'static, str>,
}

/// A diagnostic produced during compilation (lexing, parsing, or semantic analysis).
///
/// The primary underline covers `position..end` (exclusive); `end == position`
/// renders a single caret. `label` is the message shown inline under the primary
/// underline, `secondary` an optional related location, and `help` the trailing
/// `= help:` note.
#[derive(Debug)]
pub struct Diagnostic {
    pub severity: Severity,
    pub message: Cow<'static, str>,
    pub position: u32,
    pub end: u32,
    pub label: Cow<'static, str>,
    pub secondary: Option<SecondaryLabel>,
    pub help: Cow<'static, str>,
}

impl Diagnostic {
    /// Create an error-severity diagnostic anchored at a single position.
    pub fn error(
        message: impl Into<Cow<'static, str>>,
        position: u32,
        help: impl Into<Cow<'static, str>>,
    ) -> Self {
        Self {
            severity: Severity::Error,
            message: message.into(),
            position,
            end: position,
            label: Cow::Borrowed(""),
            secondary: None,
            help: help.into(),
        }
    }

    /// Create a warning-severity diagnostic anchored at a single position.
    pub fn warning(
        message: impl Into<Cow<'static, str>>,
        position: u32,
        help: impl Into<Cow<'static, str>>,
    ) -> Self {
        Self {
            severity: Severity::Warning,
            message: message.into(),
            position,
            end: position,
            label: Cow::Borrowed(""),
            secondary: None,
            help: help.into(),
        }
    }

    /// Extend the primary underline to `position..end` (exclusive byte offset).
    #[must_use]
    pub const fn with_end(mut self, end: u32) -> Self {
        self.end = end;
        self
    }

    /// Set the message shown inline under the primary underline.
    #[must_use]
    pub fn with_label(mut self, message: impl Into<Cow<'static, str>>) -> Self {
        self.label = message.into();
        self
    }

    /// Attach a secondary underline at a related location (e.g. a prior definition).
    #[must_use]
    pub fn with_secondary(
        mut self,
        start: u32,
        end: u32,
        message: impl Into<Cow<'static, str>>,
    ) -> Self {
        self.secondary = Some(SecondaryLabel {
            start,
            end,
            message: message.into(),
        });
        self
    }
}
