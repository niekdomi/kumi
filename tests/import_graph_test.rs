use kumi::lang::semantic::import_graph::{ImportEdge, ImportGraph};
use std::collections::HashMap;
use std::fs;
use std::path::Path;
use tempfile::TempDir;

/// Helper: create a temp directory with the given files and run ImportGraph::build.
/// `files` maps filename -> list of import paths (raw strings, as they'd appear in @import).
/// Returns the error messages from the graph.
fn build_graph(files: &[(&str, &[&str])]) -> (Vec<String>, Vec<String>) {
    let dir = TempDir::new().unwrap();

    // Write all files first (they need to exist for canonicalize).
    for &(name, _) in files {
        let path = dir.path().join(name);
        fs::write(&path, "").unwrap();
    }

    // Build a lookup from canonical path -> imports.
    let mut import_map: HashMap<String, Vec<String>> = HashMap::new();
    for &(name, imports) in files {
        let canonical = fs::canonicalize(dir.path().join(name))
            .unwrap()
            .to_string_lossy()
            .to_string();
        import_map.insert(
            canonical,
            imports.iter().map(|s| s.to_string()).collect(),
        );
    }

    let root_path = dir.path().join(files[0].0);
    let graph = ImportGraph::build(&root_path, |path: &Path| {
        let key = path.to_string_lossy().to_string();
        let imports = import_map.get(&key).cloned().unwrap_or_default();
        Ok(imports
            .into_iter()
            .map(|raw_path| ImportEdge {
                raw_path,
                position: 0,
            })
            .collect())
    });

    let errors: Vec<String> = graph.errors.iter().map(|d| d.message.to_string()).collect();
    let topo: Vec<String> = graph
        .topo_order
        .iter()
        .map(|p| {
            p.file_name()
                .unwrap()
                .to_string_lossy()
                .to_string()
        })
        .collect();
    (errors, topo)
}

//===---------------------------------------------------------------------===//
// Circular import detection
//===---------------------------------------------------------------------===//

#[test]
fn error_circular_import_a_b() {
    // build.kumi -> a.kumi -> b.kumi -> a.kumi (cycle)
    let (errors, _) = build_graph(&[
        ("build.kumi", &["a.kumi"]),
        ("a.kumi", &["b.kumi"]),
        ("b.kumi", &["a.kumi"]),
    ]);
    assert!(
        errors.iter().any(|e| e.contains("circular import detected")),
        "expected circular import error, got: {:?}",
        errors
    );
}

#[test]
fn error_self_import() {
    // a.kumi imports itself
    let (errors, _) = build_graph(&[("a.kumi", &["a.kumi"])]);
    assert!(
        errors.iter().any(|e| e.contains("circular import detected")),
        "expected circular import error, got: {:?}",
        errors
    );
}

#[test]
fn error_circular_import_three_files() {
    // a -> b -> c -> a
    let (errors, _) = build_graph(&[
        ("a.kumi", &["b.kumi"]),
        ("b.kumi", &["c.kumi"]),
        ("c.kumi", &["a.kumi"]),
    ]);
    assert!(
        errors.iter().any(|e| e.contains("circular import detected")),
        "expected circular import error, got: {:?}",
        errors
    );
}

//===---------------------------------------------------------------------===//
// Valid import graphs (no cycles)
//===---------------------------------------------------------------------===//

#[test]
fn valid_linear_imports() {
    // a -> b -> c (no cycle)
    let (errors, topo) = build_graph(&[
        ("a.kumi", &["b.kumi"]),
        ("b.kumi", &["c.kumi"]),
        ("c.kumi", &[]),
    ]);
    assert!(errors.is_empty(), "unexpected errors: {:?}", errors);
    // Topological order: leaves first
    assert_eq!(topo, vec!["c.kumi", "b.kumi", "a.kumi"]);
}

#[test]
fn valid_diamond_import() {
    // a -> b, a -> c, b -> d, c -> d (diamond, no cycle)
    let (errors, topo) = build_graph(&[
        ("a.kumi", &["b.kumi", "c.kumi"]),
        ("b.kumi", &["d.kumi"]),
        ("c.kumi", &["d.kumi"]),
        ("d.kumi", &[]),
    ]);
    assert!(errors.is_empty(), "unexpected errors: {:?}", errors);
    // d should come first (leaf), a should come last (root)
    assert_eq!(topo[0], "d.kumi");
    assert_eq!(topo[topo.len() - 1], "a.kumi");
}

#[test]
fn valid_no_imports() {
    let (errors, topo) = build_graph(&[("build.kumi", &[])]);
    assert!(errors.is_empty(), "unexpected errors: {:?}", errors);
    assert_eq!(topo, vec!["build.kumi"]);
}

//===---------------------------------------------------------------------===//
// Missing file
//===---------------------------------------------------------------------===//

#[test]
fn error_missing_import() {
    let (errors, _) = build_graph(&[("a.kumi", &["nonexistent.kumi"])]);
    assert!(
        errors.iter().any(|e| e.contains("cannot resolve import")),
        "expected missing file error, got: {:?}",
        errors
    );
}
