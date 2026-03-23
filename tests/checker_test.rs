use kumi::lang::lex::{Lexer, Token};
use kumi::lang::parse::Parser;
use kumi::lang::semantic::Checker;

// Helper: lex + parse + check. Returns diagnostics from the checker.
fn check(input: &str) -> Vec<String> {
    let bytes = input.as_bytes();
    let lexer = Lexer::new(bytes);
    let (tokens, lex_errors) = lexer.tokenize();
    assert!(lex_errors.is_empty(), "unexpected lex errors: {:?}", lex_errors);
    let tokens_ref: &[Token] = unsafe { std::mem::transmute(tokens.as_slice()) };
    let parser = Parser::new(tokens_ref, bytes);
    let ast = parser.parse("<test>");
    assert!(ast.errors.is_empty(), "unexpected parse errors: {:?}", ast.errors);
    let checker = Checker::new();
    let diagnostics = checker.check(&ast);
    diagnostics.into_iter().map(|d| d.message.to_string()).collect()
}

/// Check and assert no errors.
fn check_ok(input: &str) {
    let errors = check(input);
    assert!(errors.is_empty(), "unexpected errors: {:?}", errors);
}

/// Check and assert at least one error containing the given substring.
fn check_err(input: &str, expected_substr: &str) {
    let errors = check(input);
    assert!(
        errors.iter().any(|e| e.contains(expected_substr)),
        "expected error containing '{}', got: {:?}",
        expected_substr,
        errors
    );
}

//===---------------------------------------------------------------------===//
// Valid programs (no errors expected)
//===---------------------------------------------------------------------===//

#[test]
fn valid_empty_file() {
    check_ok("");
}

#[test]
fn valid_simple_project() {
    check_ok(
        r#"
        project myapp {
            version: "1.0.0";
        }
    "#,
    );
}

#[test]
fn valid_target_with_mixin() {
    check_ok(
        r#"
        mixin strict {
            warnings: "all";
        }
        target myapp with strict {
            type: executable;
        }
    "#,
    );
}

#[test]
fn valid_for_loop_with_break() {
    check_ok(
        r#"
        target myapp {
            @for file in glob("src/*.cpp") {
                @if file == "bad.cpp" {
                    @break;
                }
                sources: file;
            }
        }
    "#,
    );
}

#[test]
fn valid_nested_if() {
    check_ok(
        r#"
        target myapp {
            @if platform("linux") {
                defines: "LINUX";
            } @else-if platform("windows") {
                defines: "WINDOWS";
            } @else {
                defines: "OTHER";
            }
        }
    "#,
    );
}

#[test]
fn valid_visibility_in_target() {
    check_ok(
        r#"
        target mylib {
            public {
                include-dirs: "include";
            }
            private {
                sources: "src/lib.cpp";
            }
        }
    "#,
    );
}

#[test]
fn valid_visibility_in_mixin() {
    check_ok(
        r#"
        mixin shared {
            public {
                defines: "SHARED";
            }
        }
    "#,
    );
}

#[test]
fn valid_multiple_list_properties() {
    // List properties can appear multiple times without conflict
    check_ok(
        r#"
        target myapp {
            sources: "a.cpp";
            sources: "b.cpp";
        }
    "#,
    );
}

#[test]
fn valid_multiple_mixins_no_scalar_conflict() {
    check_ok(
        r#"
        mixin a {
            sources: "a.cpp";
        }
        mixin b {
            sources: "b.cpp";
        }
        target myapp with a, b {
            type: executable;
        }
    "#,
    );
}

#[test]
fn valid_profile_with_mixin() {
    check_ok(
        r#"
        mixin lto {
            lto: true;
        }
        profile release with lto {
            optimize: "3";
        }
    "#,
    );
}

//===---------------------------------------------------------------------===//
// Duplicate declarations
//===---------------------------------------------------------------------===//

#[test]
fn error_duplicate_target() {
    check_err(
        r#"
        target myapp { }
        target myapp { }
    "#,
        "duplicate target definition 'myapp'",
    );
}

#[test]
fn error_duplicate_mixin() {
    check_err(
        r#"
        mixin strict { }
        mixin strict { }
    "#,
        "duplicate mixin definition 'strict'",
    );
}

#[test]
fn error_duplicate_profile() {
    check_err(
        r#"
        profile release { }
        profile release { }
    "#,
        "duplicate profile definition 'release'",
    );
}

#[test]
fn error_duplicate_project() {
    check_err(
        r#"
        project a { }
        project b { }
    "#,
        "duplicate project declaration",
    );
}

//===---------------------------------------------------------------------===//
// Undefined references
//===---------------------------------------------------------------------===//

#[test]
fn error_undefined_mixin() {
    check_err(
        r#"
        target myapp with nonexistent { }
    "#,
        "undefined mixin 'nonexistent'",
    );
}

#[test]
fn error_undefined_mixin_in_profile() {
    check_err(
        r#"
        profile release with nonexistent { }
    "#,
        "undefined mixin 'nonexistent'",
    );
}

#[test]
fn error_undefined_mixin_suggests_similar() {
    let errors = check(
        r#"
        mixin strict { }
        target myapp with stric { }
    "#,
    );
    assert!(errors.len() == 1);
    assert!(errors[0].contains("undefined mixin 'stric'"));
}

//===---------------------------------------------------------------------===//
// Structural validation
//===---------------------------------------------------------------------===//

#[test]
fn error_break_outside_loop() {
    check_err(
        r#"
        target myapp {
            @break;
        }
    "#,
        "@break is only allowed inside @for loops",
    );
}

#[test]
fn error_continue_outside_loop() {
    check_err(
        r#"
        target myapp {
            @continue;
        }
    "#,
        "@continue is only allowed inside @for loops",
    );
}

#[test]
fn error_visibility_inside_for_at_top_level() {
    // Visibility blocks parsed inside @for at top level should be caught by the checker.
    check_err(
        r#"
        @for x in [a, b] {
            public {
                defines: "FOO";
            }
        }
    "#,
        "visibility blocks are only allowed inside target or mixin declarations",
    );
}

//===---------------------------------------------------------------------===//
// Property conflicts
//===---------------------------------------------------------------------===//

#[test]
fn error_duplicate_scalar_property() {
    check_err(
        r#"
        project myapp {
            version: "1.0";
            version: "2.0";
        }
    "#,
        "duplicate property 'version'",
    );
}

#[test]
fn error_mixin_scalar_conflict() {
    check_err(
        r#"
        mixin fast {
            optimize: "speed";
        }
        mixin small {
            optimize: "size";
        }
        target myapp with fast, small { }
    "#,
        "scalar property 'optimize' has conflicting values in mixin composition",
    );
}

#[test]
fn valid_mixin_same_scalar_in_one_mixin() {
    // Same mixin setting the same scalar is fine (it's not a conflict between mixins)
    check_ok(
        r#"
        mixin fast {
            optimize: "speed";
        }
        target myapp with fast {
            type: executable;
        }
    "#,
    );
}

//===---------------------------------------------------------------------===//
// Unknown functions
//===---------------------------------------------------------------------===//

#[test]
fn error_unknown_function_in_condition() {
    check_err(
        r#"
        target myapp {
            @if nonexistent_func("arg") {
                defines: "X";
            }
        }
    "#,
        "unknown function 'nonexistent_func'",
    );
}

#[test]
fn valid_known_function_in_condition() {
    check_ok(
        r#"
        target myapp {
            @if platform("linux") {
                defines: "LINUX";
            }
        }
    "#,
    );
}

#[test]
fn error_unknown_function_in_iterable() {
    check_err(
        r#"
        target myapp {
            @for file in unknown_func("*.cpp") {
                sources: file;
            }
        }
    "#,
        "unknown function 'unknown_func'",
    );
}

#[test]
fn valid_known_function_in_iterable() {
    check_ok(
        r#"
        target myapp {
            @for file in glob("src/*.cpp") {
                sources: file;
            }
        }
    "#,
    );
}

//===---------------------------------------------------------------------===//
// Duplicate singleton declarations
//===---------------------------------------------------------------------===//

#[test]
fn error_duplicate_workspace() {
    check_err(
        r#"
        workspace { }
        workspace { }
    "#,
        "duplicate workspace declaration",
    );
}

#[test]
fn error_duplicate_dependencies() {
    check_err(
        r#"
        dependencies { }
        dependencies { }
    "#,
        "duplicate dependencies declaration",
    );
}

#[test]
fn error_duplicate_options() {
    check_err(
        r#"
        options { enable_tests: true; }
        options { log_level: "info"; }
    "#,
        "duplicate options declaration",
    );
}

#[test]
fn error_duplicate_install() {
    check_err(
        r#"
        install { }
        install { }
    "#,
        "duplicate install declaration",
    );
}

#[test]
fn error_duplicate_package() {
    check_err(
        r#"
        package { }
        package { }
    "#,
        "duplicate package declaration",
    );
}

#[test]
fn error_duplicate_scripts() {
    check_err(
        r#"
        scripts { }
        scripts { }
    "#,
        "duplicate scripts declaration",
    );
}

//===---------------------------------------------------------------------===//
// Duplicate names
//===---------------------------------------------------------------------===//

#[test]
fn error_duplicate_dependency_name() {
    check_err(
        r#"
        dependencies {
            fmt: "1.0";
            fmt: "2.0";
        }
    "#,
        "duplicate dependency 'fmt'",
    );
}

#[test]
fn error_duplicate_option_name() {
    check_err(
        r#"
        options {
            enable_tests: true;
            enable_tests: false;
        }
    "#,
        "duplicate option definition 'enable_tests'",
    );
}

#[test]
fn error_duplicate_mixin_in_with_list() {
    check_err(
        r#"
        mixin a { }
        target myapp with a, a { }
    "#,
        "duplicate mixin 'a' in with list",
    );
}

//===---------------------------------------------------------------------===//
// Dependency function validation
//===---------------------------------------------------------------------===//

#[test]
fn error_unknown_dependency_function() {
    check_err(
        r#"
        dependencies {
            foo: unknown_func("bar");
        }
    "#,
        "unknown dependency function 'unknown_func'",
    );
}

#[test]
fn error_dependency_function_wrong_arg_count() {
    check_err(
        r#"
        dependencies {
            foo: git();
        }
    "#,
        "function 'git' expects 1 argument, found 0",
    );
}

#[test]
fn error_builtin_function_wrong_arg_count() {
    check_err(
        r#"
        target myapp {
            @if platform() {
                defines: "X";
            }
        }
    "#,
        "function 'platform' expects 1 argument, found 0",
    );
}

//===---------------------------------------------------------------------===//
// Loop variable shadowing
//===---------------------------------------------------------------------===//

#[test]
fn error_for_variable_shadowing() {
    check_err(
        r#"
        target myapp {
            @for x in [a, b] {
                @for x in [c, d] {
                    sources: x;
                }
            }
        }
    "#,
        "@for variable 'x' shadows an outer loop variable",
    );
}

//===---------------------------------------------------------------------===//
// Nested declarations (only allowed at top level)
//===---------------------------------------------------------------------===//

#[test]
fn error_target_inside_target() {
    check_err(
        r#"
        target outer {
            target inner { }
        }
    "#,
        "target declarations are only allowed at the top level",
    );
}

#[test]
fn error_mixin_inside_target() {
    check_err(
        r#"
        target outer {
            mixin inner { }
        }
    "#,
        "mixin declarations are only allowed at the top level",
    );
}

#[test]
fn error_profile_inside_target() {
    check_err(
        r#"
        target outer {
            profile inner { }
        }
    "#,
        "profile declarations are only allowed at the top level",
    );
}

#[test]
fn error_project_inside_target() {
    check_err(
        r#"
        target outer {
            project inner { }
        }
    "#,
        "project declarations are only allowed at the top level",
    );
}

#[test]
fn error_dependencies_inside_target() {
    check_err(
        r#"
        target outer {
            dependencies { }
        }
    "#,
        "dependencies declarations are only allowed at the top level",
    );
}

#[test]
fn error_project_inside_if() {
    check_err(
        r#"
        target myapp {
            @if platform("linux") {
                project inner { }
            }
        }
    "#,
        "project declarations are only allowed at the top level",
    );
}

#[test]
fn error_target_inside_for() {
    check_err(
        r#"
        target myapp {
            @for x in [a, b] {
                target inner { }
            }
        }
    "#,
        "target declarations are only allowed at the top level",
    );
}

//===---------------------------------------------------------------------===//
// @break/@continue at top level
//===---------------------------------------------------------------------===//

#[test]
fn error_break_at_top_level() {
    check_err(
        r#"
        @break;
    "#,
        "@break is only allowed inside @for loops",
    );
}

#[test]
fn error_continue_at_top_level() {
    check_err(
        r#"
        @continue;
    "#,
        "@continue is only allowed inside @for loops",
    );
}

//===---------------------------------------------------------------------===//
// Property conflicts (extended)
//===---------------------------------------------------------------------===//

#[test]
fn error_duplicate_scalar_in_dependency_options() {
    check_err(
        r#"
        dependencies {
            foo: "1.0" {
                tag: "v1";
                tag: "v2";
            };
        }
    "#,
        "duplicate property 'tag'",
    );
}

#[test]
fn error_mixin_scalar_conflict_in_visibility_blocks() {
    check_err(
        r#"
        mixin a {
            public {
                optimize: "speed";
            }
        }
        mixin b {
            public {
                optimize: "size";
            }
        }
        target myapp with a, b { }
    "#,
        "scalar property 'optimize' has conflicting values in mixin composition",
    );
}

//===---------------------------------------------------------------------===//
// Import validation
//===---------------------------------------------------------------------===//

#[test]
fn error_duplicate_import() {
    check_err(
        r#"
        @import "common.kumi";
        @import "common.kumi";
    "#,
        "duplicate import of 'common.kumi'",
    );
}

#[test]
fn error_import_inside_target() {
    check_err(
        r#"
        target myapp {
            @import "common.kumi";
        }
    "#,
        "@import is only allowed at the top level",
    );
}

#[test]
fn error_import_inside_mixin() {
    check_err(
        r#"
        mixin shared {
            @import "common.kumi";
        }
    "#,
        "@import is only allowed at the top level",
    );
}
