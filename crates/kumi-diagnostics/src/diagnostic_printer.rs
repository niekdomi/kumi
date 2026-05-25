use crate::{Diagnostic, Severity};
use codespan_reporting::diagnostic::{Diagnostic as CsDiagnostic, Label};
use codespan_reporting::files::SimpleFiles;
use codespan_reporting::term::termcolor::{Color, ColorChoice, ColorSpec, StandardStream};
use codespan_reporting::term::{self, Styles, StylesWriter};
use std::io::IsTerminal;
use std::ops::Range;

/// Formats and displays diagnostic messages with source context
pub struct DiagnosticPrinter<'a> {
    source: &'a str,
    files: SimpleFiles<&'a str, &'a str>,
    file_id: usize,
    config: term::Config,
    styles: Styles,
    color_choice: ColorChoice,
}

impl<'a> DiagnosticPrinter<'a> {
    pub fn new(source: &'a str, filename: &'a str) -> Self {
        let mut files = SimpleFiles::new();
        let file_id = files.add(filename, source);

        let blue = {
            let mut s = ColorSpec::new();
            s.set_fg(Some(Color::Blue)).set_intense(true).set_bold(true);
            s
        };
        // Secondary labels ("first defined here") share the blue accent but stay
        // non-bold, matching the weight of the red primary label.
        let blue_plain = ColorSpec::new().set_fg(Some(Color::Blue)).clone();

        Self {
            source,
            files,
            file_id,
            config: term::Config {
                // Underline secondary labels with '^' like primary ones (default is '-').
                chars: term::Chars {
                    single_secondary_caret: '^',
                    multi_secondary_caret_start: '^',
                    multi_secondary_caret_end: '^',
                    ..term::Chars::ascii()
                },
                ..Default::default()
            },
            styles: Styles {
                note_bullet: blue.clone(),
                primary_label_note: blue.clone(),
                source_border: blue.clone(),
                line_number: blue,
                secondary_label: blue_plain,
                ..Default::default()
            },
            color_choice: if std::io::stderr().is_terminal() {
                ColorChoice::Auto
            } else {
                ColorChoice::Never
            },
        }
    }

    /// Byte range to underline: the construct's header only — up to the first
    /// `{` or newline, trailing whitespace trimmed — so a multi-line declaration
    /// underlines just `target myapp` rather than its whole body. A zero-width
    /// span (`start == end`) falls back to a single-character caret.
    fn header_range(&self, start: u32, end: u32) -> Range<usize> {
        let start = (start as usize).min(self.source.len());
        let end = (end as usize).min(self.source.len()).max(start);
        let header =
            self.source[start..end].split(['{', '\n']).next().unwrap_or_default().trim_end();
        start..(start + header.len()).max(start + 1)
    }

    /// Prints a formatted diagnostic to stderr, styled by its severity.
    pub fn print(&self, diag: &Diagnostic) {
        let primary = Label::primary(self.file_id, self.header_range(diag.position, diag.end));
        let primary = if diag.label.is_empty() {
            primary
        } else {
            primary.with_message(diag.label.as_ref())
        };

        let mut labels = vec![primary];
        if let Some(sec) = &diag.secondary {
            labels.push(
                Label::secondary(self.file_id, self.header_range(sec.start, sec.end))
                    .with_message(sec.message.as_ref()),
            );
        }

        let codespan = match diag.severity {
            Severity::Error => CsDiagnostic::error(),
            Severity::Warning => CsDiagnostic::warning(),
        }
        .with_message(diag.message.clone())
        .with_labels(labels);

        let codespan = if diag.help.is_empty() {
            codespan
        } else {
            codespan.with_notes(vec![format!("help: {}", diag.help)])
        };

        let standard_stream = StandardStream::stderr(self.color_choice);
        let mut writer = standard_stream.lock();
        let mut style_writer = StylesWriter::new(&mut writer, &self.styles);

        term::emit_to_write_style(&mut style_writer, &self.config, &self.files, &codespan).unwrap();
    }
}
