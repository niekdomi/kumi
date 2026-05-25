use crate::{Diagnostic, Severity};
use codespan_reporting::diagnostic::{Diagnostic as CsDiagnostic, Label};
use codespan_reporting::files::SimpleFiles;
use codespan_reporting::term::termcolor::{Color, ColorChoice, ColorSpec, StandardStream};
use codespan_reporting::term::{self, Styles, StylesWriter};
use std::io::IsTerminal;

/// Formats and displays diagnostic messages with source context
pub struct DiagnosticPrinter<'a> {
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

        Self {
            files,
            file_id,
            config: term::Config {
                chars: term::Chars::ascii(),
                ..Default::default()
            },
            styles: Styles {
                note_bullet: blue.clone(),
                primary_label_note: blue.clone(),
                source_border: blue.clone(),
                line_number: blue.clone(),
                secondary_label: blue,
                ..Default::default()
            },
            color_choice: if std::io::stderr().is_terminal() {
                ColorChoice::Auto
            } else {
                ColorChoice::Never
            },
        }
    }

    /// Prints a formatted diagnostic to stderr, styled by its severity.
    pub fn print(&self, diag: &Diagnostic) {
        let pos = diag.position as usize;
        let end_pos = pos + 1;

        let cs = match diag.severity {
            Severity::Error => CsDiagnostic::error(),
            Severity::Warning => CsDiagnostic::warning(),
        };
        let mut diagnostic = cs
            .with_message(diag.message.clone())
            .with_labels(vec![Label::primary(self.file_id, pos..end_pos)]);

        if !diag.help.is_empty() {
            diagnostic = diagnostic.with_notes(vec![format!("help: {}", diag.help)]);
        }

        let standard_stream = StandardStream::stderr(self.color_choice);
        let mut writer = standard_stream.lock();
        let mut style_writer = StylesWriter::new(&mut writer, &self.styles);

        term::emit_to_write_style(&mut style_writer, &self.config, &self.files, &diagnostic)
            .unwrap();
    }
}
