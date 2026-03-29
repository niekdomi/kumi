use crate::diagnostics::Diagnostic;
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

    /// Prints a formatted error diagnostic to stderr
    pub fn print_error(&self, error: &Diagnostic) {
        let pos = error.position as usize;
        let end_pos = pos + 1;

        let mut diagnostic = CsDiagnostic::error()
            .with_message(error.message.clone())
            .with_labels(vec![Label::primary(self.file_id, pos..end_pos)]);

        if !error.help.is_empty() {
            diagnostic = diagnostic.with_notes(vec![format!("help: {}", error.help)]);
        }

        let standard_stream = StandardStream::stderr(self.color_choice);
        let mut writer = standard_stream.lock();
        let mut style_writer = StylesWriter::new(&mut writer, &self.styles);

        term::emit_to_write_style(&mut style_writer, &self.config, &self.files, &diagnostic)
            .unwrap();
    }
}
