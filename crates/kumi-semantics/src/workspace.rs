//! Workspace tree resolution (structural).
//!
//! A Kumi project is a tree of files rooted at `build.kumi`. Each file lists its
//! children via `workspace { members: "a", "b/c"; }`. This module walks that tree
//! and reports the structural errors from `semantic.md` §"Workspace Inclusion
//! Rules" — missing members, duplicate inclusion, cycles, and illegal paths.
//!
//! Cross-file *scope* inheritance (mixins/options visible down the tree) is a
//! separate, later pass; this module only establishes the set of files and that
//! the tree is well-formed.
//!
//! The filesystem is abstracted behind [`FileSource`] so tests build whole
//! multi-file projects in memory ([`MapFileSource`]) without touching disk.

use kumi_diagnostics::Diagnostic;
use kumi_syntax::ast::{Ast, Statement, Value};
use kumi_syntax::lex::Lexer;
use kumi_syntax::parse::Parser;
use std::collections::{HashMap, HashSet};
use std::path::{Component, Path, PathBuf};

/// Supplies workspace file contents, keyed by (normalized) path.
///
/// Production uses [`DiskFileSource`]; tests use [`MapFileSource`].
pub trait FileSource {
    /// The source text for `path`, or `None` if no such file exists.
    fn read(&self, path: &Path) -> Option<String>;
}

/// Reads workspace files from the real filesystem.
pub struct DiskFileSource;

impl FileSource for DiskFileSource {
    fn read(&self, path: &Path) -> Option<String> {
        std::fs::read_to_string(path).ok()
    }
}

/// In-memory [`FileSource`] for tests. Paths are normalized on insert and lookup
/// so `"./a"` and `"a"` refer to the same file.
#[derive(Default)]
pub struct MapFileSource {
    files: HashMap<PathBuf, String>,
}

impl MapFileSource {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    #[must_use]
    pub fn with(mut self, path: &str, source: &str) -> Self {
        self.files.insert(normalize(Path::new(path)), source.to_string());
        self
    }
}

impl<const N: usize> From<[(&str, &str); N]> for MapFileSource {
    fn from(entries: [(&str, &str); N]) -> Self {
        let mut files = HashMap::new();
        for (path, source) in entries {
            files.insert(normalize(Path::new(path)), source.to_string());
        }
        Self { files }
    }
}

impl FileSource for MapFileSource {
    fn read(&self, path: &Path) -> Option<String> {
        self.files.get(&normalize(path)).cloned()
    }
}

/// A diagnostic together with the file it was reported in. Workspace errors span
/// multiple files, so unlike a bare [`Diagnostic`] they carry a file path.
#[derive(Debug)]
pub struct WorkspaceError {
    pub file: PathBuf,
    pub diagnostic: Diagnostic,
}

/// The outcome of resolving a workspace tree.
#[derive(Debug, Default)]
pub struct ResolvedWorkspace {
    /// Every file in the tree, in depth-first pre-order (root first), normalized.
    pub files: Vec<PathBuf>,
    /// Structural diagnostics (and any per-file lex/parse errors encountered).
    pub errors: Vec<WorkspaceError>,
}

/// Resolve the workspace tree rooted at `root` (which must be named `build.kumi`).
pub fn resolve(root: &Path, fs: &dyn FileSource) -> ResolvedWorkspace {
    let mut out = ResolvedWorkspace::default();
    let root = normalize(root);

    if root.file_name().and_then(|n| n.to_str()) != Some("build.kumi") {
        out.errors.push(WorkspaceError {
            file: root.clone(),
            diagnostic: Diagnostic::error(
                "workspace root must be named 'build.kumi'",
                0,
                "rename the project entry file to build.kumi",
            ),
        });
        return out;
    }

    let Some(source) = fs.read(&root) else {
        out.errors.push(WorkspaceError {
            file: root.clone(),
            diagnostic: Diagnostic::error(
                format!("workspace root '{}' not found", root.display()),
                0,
                "",
            ),
        });
        return out;
    };

    let mut visited: HashSet<PathBuf> = HashSet::new();
    let mut stack: Vec<PathBuf> = Vec::new();

    visited.insert(root.clone());
    stack.push(root.clone());
    out.files.push(root.clone());
    process(&root, &source, fs, &mut out, &mut visited, &mut stack);
    stack.pop();

    out
}

/// Parse one file, extract its `workspace` members, and recurse into each.
fn process(
    path: &Path,
    source: &str,
    fs: &dyn FileSource,
    out: &mut ResolvedWorkspace,
    visited: &mut HashSet<PathBuf>,
    stack: &mut Vec<PathBuf>,
) {
    let bytes = source.as_bytes();
    let (tokens, lex_errors) = Lexer::new(bytes).tokenize();
    for diagnostic in lex_errors {
        out.errors.push(WorkspaceError {
            file: path.to_path_buf(),
            diagnostic,
        });
    }

    let mut ast = Parser::new(&tokens, bytes).parse(path.to_str().unwrap_or("<workspace>"));
    let members = collect_members(&ast);
    for diagnostic in std::mem::take(&mut ast.errors) {
        out.errors.push(WorkspaceError {
            file: path.to_path_buf(),
            diagnostic,
        });
    }
    drop(ast);

    let dir = path.parent().unwrap_or_else(|| Path::new(""));
    for member in members {
        self::resolve_member(path, dir, &member, fs, out, visited, stack);
    }
}

/// A `members:` entry: the raw path text plus its byte span in the parent file.
struct Member {
    path: String,
    start: u32,
    end: u32,
}

fn collect_members(ast: &Ast<'_>) -> Vec<Member> {
    let mut members = Vec::new();
    // Multiple `workspace` blocks in one file merge.
    for stmt in &ast.statements {
        let Statement::WorkspaceDecl(w) = stmt else {
            continue;
        };
        for prop in ast.get_properties(w.property_start_idx, w.property_end_idx) {
            if ast.get_string(prop.name_idx) != "members" {
                continue;
            }
            for &val in &ast.all_values[prop.value_start_idx as usize..prop.value_end_idx as usize]
            {
                if let Value::String(s) = val {
                    let (start, end) = ast.str_span(s);
                    members.push(Member {
                        path: s.trim_matches('"').to_string(),
                        start,
                        end,
                    });
                }
            }
        }
    }
    members
}

fn resolve_member(
    parent: &Path,
    dir: &Path,
    member: &Member,
    fs: &dyn FileSource,
    out: &mut ResolvedWorkspace,
    visited: &mut HashSet<PathBuf>,
    stack: &mut Vec<PathBuf>,
) {
    let mut err = |message: String, help: String| {
        out.errors.push(WorkspaceError {
            file: parent.to_path_buf(),
            diagnostic: Diagnostic::error(message, member.start, help)
                .with_end(member.end)
                .with_label("declared here"),
        });
    };

    let raw = Path::new(&member.path);
    if raw.is_absolute() {
        err(
            format!("workspace member '{}' must be a relative path", member.path),
            String::new(),
        );
        return;
    }
    if raw.components().any(|c| matches!(c, Component::ParentDir)) {
        err(
            format!("workspace member '{}' must not reference a parent directory", member.path),
            "members may only live in the same directory or a subdirectory".to_string(),
        );
        return;
    }

    let child = normalize(&ensure_kumi_ext(dir.join(&member.path)));

    if let Some(at) = stack.iter().position(|p| p == &child) {
        let chain: Vec<String> = stack[at..]
            .iter()
            .chain(std::iter::once(&child))
            .map(|p| p.display().to_string())
            .collect();
        err(
            format!("circular workspace inclusion of '{}'", child.display()),
            format!("inclusion chain: {}", chain.join(" -> ")),
        );
        return;
    }
    if visited.contains(&child) {
        err(
            format!("file '{}' is included in the workspace more than once", child.display()),
            "each file may appear only once in the workspace tree".to_string(),
        );
        return;
    }
    let Some(child_source) = fs.read(&child) else {
        err(format!("workspace member '{}' not found", child.display()), String::new());
        return;
    };

    visited.insert(child.clone());
    stack.push(child.clone());
    out.files.push(child.clone());
    process(&child, &child_source, fs, out, visited, stack);
    stack.pop();
}

/// Append `.kumi` unless the path already has that extension.
fn ensure_kumi_ext(path: PathBuf) -> PathBuf {
    if path.extension().and_then(|e| e.to_str()) == Some("kumi") {
        path
    } else {
        let mut s = path.into_os_string();
        s.push(".kumi");
        PathBuf::from(s)
    }
}

/// Lexically normalize a path: drop `.` and redundant separators. `..` is left
/// intact (it is rejected as an illegal member before normalization).
fn normalize(path: &Path) -> PathBuf {
    let mut out = PathBuf::new();
    for comp in path.components() {
        match comp {
            Component::CurDir => {}
            Component::ParentDir => out.push(".."),
            other => out.push(other.as_os_str()),
        }
    }
    out
}
