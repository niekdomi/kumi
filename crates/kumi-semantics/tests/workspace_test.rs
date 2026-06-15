//! Structural resolution of multi-file workspace trees.
//!
//! Every project is built in memory via `MapFileSource`, so these exercise the
//! tree/path rules from semantic.md without touching the filesystem.

use kumi_semantics::workspace::{MapFileSource, ResolvedWorkspace, resolve};
use std::path::Path;

fn resolve_str(root: &str, fs: &MapFileSource) -> ResolvedWorkspace {
    resolve(Path::new(root), fs)
}

fn file_set(ws: &ResolvedWorkspace) -> Vec<String> {
    let mut files: Vec<String> = ws.files.iter().map(|p| p.display().to_string()).collect();
    files.sort();
    files
}

fn messages(ws: &ResolvedWorkspace) -> Vec<String> {
    ws.errors.iter().map(|e| e.diagnostic.message.to_string()).collect()
}

#[track_caller]
fn assert_clean(ws: &ResolvedWorkspace) {
    assert!(ws.errors.is_empty(), "unexpected workspace errors: {:?}", messages(ws));
}

#[track_caller]
fn assert_err_contains(ws: &ResolvedWorkspace, substr: &str) {
    assert!(
        messages(ws).iter().any(|m| m.contains(substr)),
        "expected an error containing '{substr}', got: {:?}",
        messages(ws),
    );
}

//===---------------------------------------------------------------------===//
// Well-formed trees
//===---------------------------------------------------------------------===//

#[test]
fn flat_inclusion() {
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "lib", "app"; }"#),
        ("lib.kumi", r#"target lib { type: "static_library"; }"#),
        ("app.kumi", "target app { type: executable; }"),
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_clean(&ws);
    assert_eq!(file_set(&ws), vec!["app.kumi", "build.kumi", "lib.kumi"]);
}

#[test]
fn nested_inclusion() {
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "lib/core"; }"#),
        ("lib/core.kumi", r#"workspace { members: "util"; } target core { }"#),
        ("lib/util.kumi", "target util { }"),
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_clean(&ws);
    assert_eq!(file_set(&ws), vec!["build.kumi", "lib/core.kumi", "lib/util.kumi"]);
}

#[test]
fn mixed_inclusion() {
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "app", "lib/core"; }"#),
        ("app.kumi", "target app { }"),
        ("lib/core.kumi", r#"workspace { members: "sub/helper"; }"#),
        ("lib/sub/helper.kumi", "target helper { }"),
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_clean(&ws);
    assert_eq!(ws.files.len(), 4);
}

#[test]
fn kumi_extension_optional() {
    // Members may omit `.kumi`; an explicit extension is also accepted.
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "lib", "app.kumi"; }"#),
        ("lib.kumi", "target lib { }"),
        ("app.kumi", "target app { }"),
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_clean(&ws);
    assert_eq!(ws.files.len(), 3);
}

#[test]
fn leaf_without_workspace_block() {
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "lib"; }"#),
        ("lib.kumi", "target lib { }"), // no workspace block -> leaf
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_clean(&ws);
    assert_eq!(ws.files.len(), 2);
}

#[test]
fn multiple_workspace_blocks_merge() {
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "a"; } workspace { members: "b"; }"#),
        ("a.kumi", "target a { }"),
        ("b.kumi", "target b { }"),
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_clean(&ws);
    assert_eq!(ws.files.len(), 3);
}

#[test]
fn empty_root_has_no_members() {
    let fs = MapFileSource::from([("build.kumi", "project myapp { }")]);
    let ws = resolve_str("build.kumi", &fs);
    assert_clean(&ws);
    assert_eq!(file_set(&ws), vec!["build.kumi"]);
}

//===---------------------------------------------------------------------===//
// Missing files
//===---------------------------------------------------------------------===//

#[test]
fn missing_root() {
    let ws = resolve_str("build.kumi", &MapFileSource::new());
    assert_err_contains(&ws, "not found");
}

#[test]
fn missing_member() {
    let fs = MapFileSource::from([("build.kumi", r#"workspace { members: "ghost"; }"#)]);
    let ws = resolve_str("build.kumi", &fs);
    assert_err_contains(&ws, "workspace member 'ghost.kumi' not found");
}

//===---------------------------------------------------------------------===//
// Illegal paths
//===---------------------------------------------------------------------===//

#[test]
fn root_must_be_build_kumi() {
    let fs = MapFileSource::from([("main.kumi", "project p { }")]);
    let ws = resolve_str("main.kumi", &fs);
    assert_err_contains(&ws, "must be named 'build.kumi'");
}

#[test]
fn upward_reference_rejected() {
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "../escape"; }"#),
        ("../escape.kumi", "target x { }"), // exists, but still illegal
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_err_contains(&ws, "must not reference a parent directory");
}

#[test]
fn nested_upward_reference_rejected() {
    // A nested file using `../` to climb out of its own directory is rejected as
    // an upward reference — caught before any cycle/duplicate analysis, and
    // attributed to the nested file rather than the root.
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "lib/core"; }"#),
        ("lib/core.kumi", r#"workspace { members: "../xx"; }"#),
        ("xx.kumi", "target xx { }"), // exists at the root, but is unreachable legally
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_err_contains(&ws, "must not reference a parent directory");
    assert!(
        ws.errors.iter().any(|e| e.file.display().to_string() == "lib/core.kumi"),
        "error should be attributed to lib/core.kumi, got: {:?}",
        ws.errors.iter().map(|e| e.file.display().to_string()).collect::<Vec<_>>(),
    );
    // `xx.kumi` is never pulled in, since the only path to it was illegal.
    assert!(!ws.files.iter().any(|p| p.display().to_string() == "xx.kumi"));
}

#[test]
fn same_directory_cycle() {
    // With upward `../` forbidden, cross-directory cycles are impossible; the
    // only cycles are within one directory. a -> b -> a here.
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "lib/a"; }"#),
        ("lib/a.kumi", r#"workspace { members: "b"; }"#),
        ("lib/b.kumi", r#"workspace { members: "a"; }"#),
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_err_contains(&ws, "circular workspace inclusion");
}

#[test]
fn absolute_path_rejected() {
    let fs = MapFileSource::from([("build.kumi", r#"workspace { members: "/etc/passwd"; }"#)]);
    let ws = resolve_str("build.kumi", &fs);
    assert_err_contains(&ws, "must be a relative path");
}

#[test]
fn dot_segments_are_normalized() {
    // "./lib" and "lib" denote the same file -> the second is a duplicate.
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "lib", "./lib"; }"#),
        ("lib.kumi", "target lib { }"),
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_err_contains(&ws, "more than once");
}

//===---------------------------------------------------------------------===//
// Duplicate inclusion & cycles
//===---------------------------------------------------------------------===//

#[test]
fn duplicate_inclusion_diamond() {
    // build -> {a, b}, and both a and b include `shared`.
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "a", "b"; }"#),
        ("a.kumi", r#"workspace { members: "shared"; }"#),
        ("b.kumi", r#"workspace { members: "shared"; }"#),
        ("shared.kumi", "target shared { }"),
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_err_contains(&ws, "included in the workspace more than once");
}

#[test]
fn cycle_back_to_root() {
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "a"; }"#),
        ("a.kumi", r#"workspace { members: "build"; }"#),
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_err_contains(&ws, "circular workspace inclusion");
}

#[test]
fn cycle_indirect() {
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "a"; }"#),
        ("a.kumi", r#"workspace { members: "b"; }"#),
        ("b.kumi", r#"workspace { members: "a"; }"#),
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert_err_contains(&ws, "circular workspace inclusion");
}

//===---------------------------------------------------------------------===//
// Per-file parse errors are surfaced (attributed to the offending file)
//===---------------------------------------------------------------------===//

#[test]
fn member_parse_error_is_surfaced() {
    let fs = MapFileSource::from([
        ("build.kumi", r#"workspace { members: "broken"; }"#),
        ("broken.kumi", "target { }"), // missing target name -> parse error
    ]);
    let ws = resolve_str("build.kumi", &fs);
    assert!(
        ws.errors.iter().any(|e| e.file.display().to_string() == "broken.kumi"),
        "expected an error attributed to broken.kumi, got: {:?}",
        messages(&ws),
    );
}
