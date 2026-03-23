// Import graph construction, circular import detection, and topological sorting.
//
// Processes @import statements across multiple kumi files to produce a
// dependency-ordered list of files for semantic analysis.
//
// Modeled after Go's import resolution:
//   - Each file is parsed exactly once (cached by canonical path).
//   - Circular imports are detected via 3-color DFS.
//   - Files are returned in topological order (leaves first) so that
//     all imported symbols are available before they are referenced.

use crate::diagnostics::Diagnostic;
use crate::lang::ast::{Ast, Statement};
use std::collections::HashMap;
use std::path::{Path, PathBuf};

/// State of a file during DFS traversal.
#[derive(Clone, Copy, PartialEq, Eq)]
enum FileState {
    /// Currently being processed (on the DFS stack).
    InProgress,
    /// Fully processed.
    Done,
}

/// Maximum import depth before we bail out (defense-in-depth).
const MAX_IMPORT_DEPTH: usize = 64;

/// Represents the import dependency graph across all kumi files.
pub struct ImportGraph {
    /// Canonical path → file state during construction, final ordering after build.
    states: HashMap<PathBuf, FileState>,
    /// Topologically sorted file paths (leaves first).
    pub topo_order: Vec<PathBuf>,
    /// Errors accumulated during graph construction.
    pub errors: Vec<Diagnostic>,
}

impl ImportGraph {
    /// Build an import graph starting from `root_file`.
    ///
    /// The `parse_file` callback is invoked for each file that needs to be
    /// parsed. It receives the canonical path and must return the parsed AST.
    /// The caller owns the ASTs and manages their lifetimes.
    ///
    /// Returns the import graph with topological ordering and any errors
    /// (circular imports, missing files, etc.).
    pub fn build<F>(root_file: &Path, mut parse_file: F) -> Self
    where
        F: FnMut(&Path) -> Result<Vec<ImportEdge>, Diagnostic>,
    {
        let mut graph = Self {
            states: HashMap::new(),
            topo_order: Vec::new(),
            errors: Vec::new(),
        };

        let root = match std::fs::canonicalize(root_file) {
            Ok(p) => p,
            Err(e) => {
                graph.errors.push(Diagnostic::new(
                    format!("cannot open '{}': {}", root_file.display(), e),
                    0,
                    "",
                ));
                return graph;
            }
        };

        let mut stack: Vec<PathBuf> = Vec::new();
        graph.visit(&root, &mut stack, &mut parse_file);
        graph
    }

    fn visit<F>(&mut self, path: &Path, stack: &mut Vec<PathBuf>, parse_file: &mut F)
    where
        F: FnMut(&Path) -> Result<Vec<ImportEdge>, Diagnostic>,
    {
        match self.states.get(path) {
            Some(FileState::Done) => return,
            Some(FileState::InProgress) => {
                // Circular import detected — build the chain for the error message.
                let cycle_start = stack.iter().position(|p| p == path).unwrap();
                let chain: Vec<String> = stack[cycle_start..]
                    .iter()
                    .map(|p| p.display().to_string())
                    .chain(std::iter::once(path.display().to_string()))
                    .collect();

                self.errors.push(Diagnostic::new(
                    format!("circular import detected"),
                    0,
                    format!("import chain: {}", chain.join(" -> ")),
                ));
                return;
            }
            _ => {}
        }

        if stack.len() >= MAX_IMPORT_DEPTH {
            self.errors.push(Diagnostic::new(
                format!("import chain exceeds {} levels", MAX_IMPORT_DEPTH),
                0,
                "reduce nesting or restructure imports",
            ));
            return;
        }

        self.states.insert(path.to_path_buf(), FileState::InProgress);
        stack.push(path.to_path_buf());

        match parse_file(path) {
            Ok(edges) => {
                for edge in edges {
                    let import_path = match resolve_import_path(&edge.raw_path, path) {
                        Ok(p) => p,
                        Err(e) => {
                            self.errors.push(Diagnostic::new(
                                format!("cannot resolve import '{}': {}", edge.raw_path, e),
                                edge.position,
                                "",
                            ));
                            continue;
                        }
                    };
                    self.visit(&import_path, stack, parse_file);
                }
            }
            Err(diag) => {
                self.errors.push(diag);
            }
        }

        stack.pop();
        self.states.insert(path.to_path_buf(), FileState::Done);
        self.topo_order.push(path.to_path_buf());
    }
}

/// An import edge extracted from an AST.
pub struct ImportEdge {
    /// The raw path string from the @import statement.
    pub raw_path: String,
    /// Byte offset of the @import statement (for error reporting).
    pub position: u32,
}

/// Extract all @import edges from an AST.
pub fn extract_imports(ast: &Ast) -> Vec<ImportEdge> {
    let mut edges = Vec::new();
    for stmt in &ast.statements {
        if let Statement::ImportStmt(import) = stmt {
            let raw = ast.get_string(import.path_idx);
            // Strip surrounding quotes if present (parser stores the raw string content).
            let path = raw.trim_matches('"');
            edges.push(ImportEdge {
                raw_path: path.to_string(),
                position: import.base.start_idx,
            });
        }
    }
    edges
}

/// Resolve a relative import path against the importing file's directory.
fn resolve_import_path(
    import_path: &str,
    importing_file: &Path,
) -> Result<PathBuf, std::io::Error> {
    let dir = importing_file.parent().unwrap_or_else(|| Path::new("."));
    let resolved = dir.join(import_path);
    std::fs::canonicalize(&resolved)
}
