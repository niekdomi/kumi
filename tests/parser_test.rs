use kumi::lang::ast::*;
use kumi::lang::lex::{Lexer, Token};
use kumi::lang::parse::Parser;

// Helper: lex + parse. Returns (tokens, ast) because the AST borrows from tokens.
fn parse(input: &str) -> (Vec<Token>, Ast<'_>) {
    let bytes = input.as_bytes();
    let lexer = Lexer::new(bytes);
    let (tokens, lex_errors) = lexer.tokenize();
    assert!(lex_errors.is_empty(), "unexpected lex errors: {:?}", lex_errors);
    // SAFETY: tokens Vec won't be moved/dropped while ast is alive since we return both.
    // We need this because Parser borrows &[Token] but Ast outlives the parse call.
    let tokens_ref: &[Token] = unsafe { std::mem::transmute(tokens.as_slice()) };
    let parser = Parser::new(tokens_ref, bytes);
    let ast = parser.parse("<test>");
    (tokens, ast)
}

/// Parse and assert no errors. Use via: `let (_tokens, ast) = parse_ok("...");`
fn parse_ok(input: &str) -> (Vec<Token>, Ast<'_>) {
    let result = parse(input);
    assert!(result.1.errors.is_empty(), "unexpected parse errors: {:?}", result.1.errors);
    result
}

/// Parse and assert at least one error.
fn parse_err(input: &str) -> (Vec<Token>, Ast<'_>) {
    let result = parse(input);
    assert!(!result.1.errors.is_empty(), "expected parse error but succeeded");
    result
}

//===---------------------------------------------------------------------===//
// Top-Level Declarations
//===---------------------------------------------------------------------===//

#[test]
fn project_simple() {
    let (_tokens, ast) = parse_ok("project myapp { }");
    assert_eq!(ast.statements.len(), 1);
    let Statement::ProjectDecl(p) = ast.statements[0] else {
        panic!("expected ProjectDecl")
    };
    assert_eq!(ast.get_string(p.name_idx), "myapp");
    assert_eq!(p.property_start_idx, p.property_end_idx);
}

#[test]
fn project_with_properties() {
    let (_tokens, ast) = parse_ok(
        r#"
        project myapp {
            version: "1.0.0";
            standard: "c++20";
        }"#,
    );
    let Statement::ProjectDecl(p) = ast.statements[0] else {
        panic!("expected ProjectDecl")
    };
    assert_eq!(ast.get_string(p.name_idx), "myapp");

    let props = ast.get_properties(p.property_start_idx, p.property_end_idx);
    assert_eq!(props.len(), 2);
    assert_eq!(ast.get_string(props[0].name_idx), "version");
    assert_eq!(*ast.get_value(props[0].value_start_idx), Value::String("\"1.0.0\""));
    assert_eq!(ast.get_string(props[1].name_idx), "standard");
    assert_eq!(*ast.get_value(props[1].value_start_idx), Value::String("\"c++20\""));
}

#[test]
fn workspace_simple() {
    let (_tokens, ast) = parse_ok(
        r#"
        workspace {
            build_dir: "build";
        }"#,
    );
    assert_eq!(ast.statements.len(), 1);
    let Statement::WorkspaceDecl(w) = ast.statements[0] else {
        panic!("expected WorkspaceDecl")
    };
    let props = ast.get_properties(w.property_start_idx, w.property_end_idx);
    assert_eq!(props.len(), 1);
    assert_eq!(ast.get_string(props[0].name_idx), "build_dir");
}

#[test]
fn target_simple() {
    let (_tokens, ast) = parse_ok(
        r#"
        target mylib {
            type: "static_library";
        }"#,
    );
    assert_eq!(ast.statements.len(), 1);
    let Statement::TargetDecl(t) = ast.statements[0] else {
        panic!("expected TargetDecl")
    };
    assert_eq!(ast.get_string(t.name_idx), "mylib");
    assert_eq!(t.mixin_start_idx, t.mixin_end_idx); // no mixins

    let body = ast.get_statements(t.body_start_idx, t.body_end_idx);
    assert_eq!(body.len(), 1);
    let Statement::Property(prop) = body[0] else {
        panic!("expected Property")
    };
    assert_eq!(ast.get_string(prop.name_idx), "type");
}

#[test]
fn target_with_mixins() {
    let (_tokens, ast) = parse_ok("target myapp with strict, warnings { }");
    let Statement::TargetDecl(t) = ast.statements[0] else {
        panic!("expected TargetDecl")
    };
    assert_eq!(ast.get_string(t.name_idx), "myapp");

    let mixins: Vec<&str> =
        (t.mixin_start_idx..t.mixin_end_idx).map(|i| ast.get_string(i)).collect();
    assert_eq!(mixins, vec!["strict", "warnings"]);
}

#[test]
fn target_with_visibility_blocks() {
    let (_tokens, ast) = parse_ok(
        r#"
        target mylib {
            public {
                include_dirs: "include";
            }
            private {
                sources: "src/main.cpp";
            }
        }"#,
    );
    let Statement::TargetDecl(t) = ast.statements[0] else {
        panic!("expected TargetDecl")
    };
    let body = ast.get_statements(t.body_start_idx, t.body_end_idx);
    assert_eq!(body.len(), 2);

    let Statement::VisibilityBlock(pub_block) = body[0] else {
        panic!("expected VisibilityBlock")
    };
    assert_eq!(pub_block.visibility, Visibility::Public);
    let pub_props = ast.get_properties(pub_block.property_start_idx, pub_block.property_end_idx);
    assert_eq!(pub_props.len(), 1);
    assert_eq!(ast.get_string(pub_props[0].name_idx), "include_dirs");

    let Statement::VisibilityBlock(priv_block) = body[1] else {
        panic!("expected VisibilityBlock")
    };
    assert_eq!(priv_block.visibility, Visibility::Private);
}

#[test]
fn install_decl() {
    let (_tokens, ast) = parse_ok(
        r#"
        install {
            bin_dir: "bin";
            lib_dir: "lib";
        }"#,
    );
    assert_eq!(ast.statements.len(), 1);
    let Statement::InstallDecl(i) = ast.statements[0] else {
        panic!("expected InstallDecl")
    };
    let props = ast.get_properties(i.property_start_idx, i.property_end_idx);
    assert_eq!(props.len(), 2);
}

#[test]
fn package_decl() {
    let (_tokens, ast) = parse_ok(
        r#"
        package {
            name: "myapp";
            license: "MIT";
        }"#,
    );
    assert_eq!(ast.statements.len(), 1);
    let Statement::PackageDecl(p) = ast.statements[0] else {
        panic!("expected PackageDecl")
    };
    let props = ast.get_properties(p.property_start_idx, p.property_end_idx);
    assert_eq!(props.len(), 2);
}

#[test]
fn script_decl() {
    let (_tokens, ast) = parse_ok(
        r#"
        script {
            name: "generate";
            command: "python gen.py";
        }"#,
    );
    assert_eq!(ast.statements.len(), 1);
    let Statement::ScriptDecl(s) = ast.statements[0] else {
        panic!("expected ScriptDecl")
    };
    let props = ast.get_properties(s.property_start_idx, s.property_end_idx);
    assert_eq!(props.len(), 2);
}

//===---------------------------------------------------------------------===//
// Dependencies
//===---------------------------------------------------------------------===//

#[test]
fn dependencies_version_string() {
    let (_tokens, ast) = parse_ok(
        r#"
        dependencies {
            fmt: "10.1.0";
        }"#,
    );
    let Statement::DependenciesDecl(d) = ast.statements[0] else {
        panic!("expected DependenciesDecl")
    };
    let deps = ast.get_dependencies(d.dep_start_idx, d.dep_end_idx);
    assert_eq!(deps.len(), 1);
    assert_eq!(ast.get_string(deps[0].name_idx), "fmt");
    assert!(!deps[0].is_optional);
    assert!(matches!(deps[0].value, DependencyValue::String("\"10.1.0\"")));
}

#[test]
fn dependencies_optional() {
    let (_tokens, ast) = parse_ok(
        r#"
        dependencies {
            vulkan?: system;
        }"#,
    );
    let Statement::DependenciesDecl(d) = ast.statements[0] else {
        panic!("expected DependenciesDecl")
    };
    let deps = ast.get_dependencies(d.dep_start_idx, d.dep_end_idx);
    assert_eq!(deps.len(), 1);
    assert!(deps[0].is_optional);
    assert_eq!(ast.get_string(deps[0].name_idx), "vulkan");
    // system is modeled as a FunctionCall with no args
    assert!(matches!(deps[0].value, DependencyValue::FunctionCall(_)));
}

#[test]
fn dependencies_function_call_with_options() {
    let (_tokens, ast) = parse_ok(
        r#"
        dependencies {
            spdlog: git("https://github.com/gabime/spdlog.git") {
                tag: "v1.12.0";
            };
        }"#,
    );
    let Statement::DependenciesDecl(d) = ast.statements[0] else {
        panic!("expected DependenciesDecl")
    };
    let deps = ast.get_dependencies(d.dep_start_idx, d.dep_end_idx);
    assert_eq!(deps.len(), 1);
    assert_eq!(ast.get_string(deps[0].name_idx), "spdlog");

    let DependencyValue::FunctionCall(fc) = deps[0].value else {
        panic!("expected FunctionCall")
    };
    assert_eq!(ast.get_string(fc.name_idx), "git");

    let opts = ast.get_properties(deps[0].option_start_idx, deps[0].option_end_idx);
    assert_eq!(opts.len(), 1);
    assert_eq!(ast.get_string(opts[0].name_idx), "tag");
}

#[test]
fn dependencies_multiple() {
    let (_tokens, ast) = parse_ok(
        r#"
        dependencies {
            fmt: "10.1.0";
            spdlog: "1.12.0";
            vulkan?: system;
        }"#,
    );
    let Statement::DependenciesDecl(d) = ast.statements[0] else {
        panic!("expected DependenciesDecl")
    };
    let deps = ast.get_dependencies(d.dep_start_idx, d.dep_end_idx);
    assert_eq!(deps.len(), 3);
}

//===---------------------------------------------------------------------===//
// Options
//===---------------------------------------------------------------------===//

#[test]
fn options_simple() {
    let (_tokens, ast) = parse_ok(
        r#"
        options {
            enable_tests: true;
            log_level: "info";
        }"#,
    );
    let Statement::OptionsDecl(o) = ast.statements[0] else {
        panic!("expected OptionsDecl")
    };
    let opts = ast.get_options(o.option_start_idx, o.option_end_idx);
    assert_eq!(opts.len(), 2);
    assert_eq!(ast.get_string(opts[0].name_idx), "enable_tests");
    assert_eq!(opts[0].default_value, Value::Boolean(true));
    assert_eq!(ast.get_string(opts[1].name_idx), "log_level");
    assert_eq!(opts[1].default_value, Value::String("\"info\""));
}

#[test]
fn options_with_constraints() {
    let (_tokens, ast) = parse_ok(
        r#"
        options {
            max_threads: 8 {
                min: 1;
                max: 128;
            };
        }"#,
    );
    let Statement::OptionsDecl(o) = ast.statements[0] else {
        panic!("expected OptionsDecl")
    };
    let opts = ast.get_options(o.option_start_idx, o.option_end_idx);
    assert_eq!(opts.len(), 1);
    assert_eq!(opts[0].default_value, Value::Integer(8));

    let constraints = ast.get_properties(opts[0].constraint_start_idx, opts[0].constraint_end_idx);
    assert_eq!(constraints.len(), 2);
    assert_eq!(ast.get_string(constraints[0].name_idx), "min");
    assert_eq!(ast.get_string(constraints[1].name_idx), "max");
}

//===---------------------------------------------------------------------===//
// Mixin
//===---------------------------------------------------------------------===//

#[test]
fn mixin_simple() {
    let (_tokens, ast) = parse_ok(
        r#"
        mixin strict {
            warnings: "all";
            werror: true;
        }"#,
    );
    let Statement::MixinDecl(m) = ast.statements[0] else {
        panic!("expected MixinDecl")
    };
    assert_eq!(ast.get_string(m.name_idx), "strict");
    let body = ast.get_statements(m.body_start_idx, m.body_end_idx);
    assert_eq!(body.len(), 2);
}

//===---------------------------------------------------------------------===//
// Profile
//===---------------------------------------------------------------------===//

#[test]
fn profile_simple() {
    let (_tokens, ast) = parse_ok(
        r#"
        profile release {
            optimization: "3";
            lto: true;
        }"#,
    );
    let Statement::ProfileDecl(p) = ast.statements[0] else {
        panic!("expected ProfileDecl")
    };
    assert_eq!(ast.get_string(p.name_idx), "release");
    assert_eq!(p.mixin_start_idx, p.mixin_end_idx); // no mixins
    let props = ast.get_properties(p.property_start_idx, p.property_end_idx);
    assert_eq!(props.len(), 2);
}

#[test]
fn profile_with_mixins() {
    let (_tokens, ast) = parse_ok(
        r#"
        profile release with strict {
            optimization: "3";
        }"#,
    );
    let Statement::ProfileDecl(p) = ast.statements[0] else {
        panic!("expected ProfileDecl")
    };
    let mixins: Vec<&str> =
        (p.mixin_start_idx..p.mixin_end_idx).map(|i| ast.get_string(i)).collect();
    assert_eq!(mixins, vec!["strict"]);
}

//===---------------------------------------------------------------------===//
// Control Flow
//===---------------------------------------------------------------------===//

#[test]
fn if_simple() {
    let (_tokens, ast) = parse_ok(
        r#"
        @if platform == "linux" {
            defines: "PLATFORM_LINUX";
        }"#,
    );
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };

    assert!(matches!(if_stmt.condition, Condition::ComparisonExpr(_)));
    let then_stmts = ast.get_statements(if_stmt.then_start_idx, if_stmt.then_end_idx);
    assert_eq!(then_stmts.len(), 1);
    // no else
    assert_eq!(if_stmt.else_start_idx, if_stmt.else_end_idx);
}

#[test]
fn if_else() {
    let (_tokens, ast) = parse_ok(
        r#"
        @if platform == "linux" {
            defines: "LINUX";
        } @else {
            defines: "OTHER";
        }"#,
    );
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };

    let then_stmts = ast.get_statements(if_stmt.then_start_idx, if_stmt.then_end_idx);
    assert_eq!(then_stmts.len(), 1);
    let else_stmts = ast.get_statements(if_stmt.else_start_idx, if_stmt.else_end_idx);
    assert_eq!(else_stmts.len(), 1);
}

#[test]
fn if_else_if_else() {
    let (_tokens, ast) = parse_ok(
        r#"
        @if platform == "linux" {
            defines: "LINUX";
        } @else-if platform == "windows" {
            defines: "WIN32";
        } @else {
            defines: "UNKNOWN";
        }"#,
    );
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };

    // else branch contains another IfStmt
    let else_stmts = ast.get_statements(if_stmt.else_start_idx, if_stmt.else_end_idx);
    assert_eq!(else_stmts.len(), 1);
    assert!(matches!(else_stmts[0], Statement::IfStmt(_)));
}

#[test]
fn if_logical_and() {
    let (_tokens, ast) = parse_ok(
        r#"
        @if platform == "linux" and arch == "x86_64" {
            defines: "X86_LINUX";
        }"#,
    );
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };
    let Condition::LogicalExpr(logical) = if_stmt.condition else {
        panic!("expected LogicalExpr")
    };
    assert_eq!(logical.op, LogicalOperator::And);
    assert_eq!(logical.operand_end_idx - logical.operand_start_idx, 2);
}

#[test]
fn if_logical_or() {
    let (_tokens, ast) = parse_ok(
        r#"
        @if platform == "linux" or platform == "macos" {
            defines: "UNIX";
        }"#,
    );
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };
    let Condition::LogicalExpr(logical) = if_stmt.condition else {
        panic!("expected LogicalExpr")
    };
    assert_eq!(logical.op, LogicalOperator::Or);
}

#[test]
fn if_negated() {
    let (_tokens, ast) = parse_ok(
        r#"
        @if not debug {
            optimization: "3";
        }"#,
    );
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };
    let Condition::UnaryExpr(unary) = if_stmt.condition else {
        panic!("expected UnaryExpr")
    };
    assert!(unary.is_negated);
}

#[test]
fn if_function_call_condition() {
    let (_tokens, ast) = parse_ok(
        r#"
        @if has_feature("avx2") {
            defines: "USE_AVX2";
        }"#,
    );
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };
    let Condition::UnaryExpr(unary) = if_stmt.condition else {
        panic!("expected UnaryExpr")
    };
    assert!(!unary.is_negated);
    assert_eq!(unary.operand.kind, OperandType::FunctionCall);
}

#[test]
fn if_parenthesized_condition() {
    let (_tokens, ast) = parse_ok(
        r#"
        @if (debug or sanitize) and not release {
            defines: "SAFE";
        }"#,
    );
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };
    assert!(matches!(if_stmt.condition, Condition::LogicalExpr(_)));
}

//===---------------------------------------------------------------------===//
// For Loops
//===---------------------------------------------------------------------===//

#[test]
fn for_loop_with_list() {
    let (_tokens, ast) = parse_ok(
        r#"
        @for flag in ["-Wall", "-Wextra"] {
            cflags: flag;
        }"#,
    );
    let Statement::ForStmt(for_stmt) = ast.statements[0] else {
        panic!("expected ForStmt")
    };
    assert_eq!(ast.get_string(for_stmt.variable_name_idx), "flag");
    assert!(matches!(for_stmt.iterable, Iterable::List(_)));

    let Iterable::List(list) = for_stmt.iterable else {
        unreachable!()
    };
    assert_eq!(list.element_end_idx - list.element_start_idx, 2);

    let body = ast.get_statements(for_stmt.body_start_idx, for_stmt.body_end_idx);
    assert_eq!(body.len(), 1);
}

#[test]
fn for_loop_with_range() {
    let (_tokens, ast) = parse_ok(
        r#"
        @for i in 0..10 {
            defines: "LEVEL";
        }"#,
    );
    let Statement::ForStmt(for_stmt) = ast.statements[0] else {
        panic!("expected ForStmt")
    };
    assert_eq!(ast.get_string(for_stmt.variable_name_idx), "i");

    let Iterable::Range(range) = for_stmt.iterable else {
        panic!("expected Range")
    };
    assert_eq!(range.start_idx, 0);
    assert_eq!(range.end_idx, 10);
}

#[test]
fn for_loop_with_function_call() {
    let (_tokens, ast) = parse_ok(
        r#"
        @for file in files("src/*.cpp") {
            sources: file;
        }"#,
    );
    let Statement::ForStmt(for_stmt) = ast.statements[0] else {
        panic!("expected ForStmt")
    };
    assert!(matches!(for_stmt.iterable, Iterable::FunctionCall(_)));
}

#[test]
fn loop_control_break() {
    let (_tokens, ast) = parse_ok(
        r#"
        @for file in files("src/*.cpp") {
            @break;
        }"#,
    );
    let Statement::ForStmt(for_stmt) = ast.statements[0] else {
        panic!("expected ForStmt")
    };
    let body = ast.get_statements(for_stmt.body_start_idx, for_stmt.body_end_idx);
    assert_eq!(body.len(), 1);
    let Statement::LoopControlStmt(ctrl) = body[0] else {
        panic!("expected LoopControlStmt")
    };
    assert_eq!(ctrl.control, LoopControl::Break);
}

#[test]
fn loop_control_continue() {
    let (_tokens, ast) = parse_ok(
        r#"
        @for file in files("src/*.cpp") {
            @continue;
        }"#,
    );
    let Statement::ForStmt(for_stmt) = ast.statements[0] else {
        panic!("expected ForStmt")
    };
    let body = ast.get_statements(for_stmt.body_start_idx, for_stmt.body_end_idx);
    let Statement::LoopControlStmt(ctrl) = body[0] else {
        panic!("expected LoopControlStmt")
    };
    assert_eq!(ctrl.control, LoopControl::Continue);
}

//===---------------------------------------------------------------------===//
// Diagnostic Directives
//===---------------------------------------------------------------------===//

#[test]
fn diagnostic_error() {
    let (_tokens, ast) = parse_ok(r#"@error "unsupported platform";"#);
    let Statement::DiagnosticStmt(d) = ast.statements[0] else {
        panic!("expected DiagnosticStmt")
    };
    assert_eq!(d.level, DiagnosticLevel::Error);
    assert_eq!(ast.get_string(d.message_idx), "\"unsupported platform\"");
}

#[test]
fn diagnostic_warning() {
    let (_tokens, ast) = parse_ok(r#"@warning "feature X is deprecated";"#);
    let Statement::DiagnosticStmt(d) = ast.statements[0] else {
        panic!("expected DiagnosticStmt")
    };
    assert_eq!(d.level, DiagnosticLevel::Warning);
}

#[test]
fn diagnostic_info() {
    let (_tokens, ast) = parse_ok(r#"@info "building with debug symbols";"#);
    let Statement::DiagnosticStmt(d) = ast.statements[0] else {
        panic!("expected DiagnosticStmt")
    };
    assert_eq!(d.level, DiagnosticLevel::Info);
}

#[test]
fn diagnostic_debug() {
    let (_tokens, ast) = parse_ok(r#"@debug "verbose trace enabled";"#);
    let Statement::DiagnosticStmt(d) = ast.statements[0] else {
        panic!("expected DiagnosticStmt")
    };
    assert_eq!(d.level, DiagnosticLevel::Debug);
}

//===---------------------------------------------------------------------===//
// Properties
//===---------------------------------------------------------------------===//

#[test]
fn property_single_string_value() {
    let (_tokens, ast) = parse_ok(
        r#"
        project myapp {
            version: "1.0.0";
        }"#,
    );
    let Statement::ProjectDecl(p) = ast.statements[0] else {
        panic!("expected ProjectDecl")
    };
    let props = ast.get_properties(p.property_start_idx, p.property_end_idx);
    assert_eq!(props.len(), 1);
    assert_eq!(props[0].value_end_idx - props[0].value_start_idx, 1);
    assert_eq!(*ast.get_value(props[0].value_start_idx), Value::String("\"1.0.0\""));
}

#[test]
fn property_multiple_values() {
    let (_tokens, ast) = parse_ok(
        r#"
        project myapp {
            sources: "main.cpp", "util.cpp";
        }"#,
    );
    let Statement::ProjectDecl(p) = ast.statements[0] else {
        panic!("expected ProjectDecl")
    };
    let props = ast.get_properties(p.property_start_idx, p.property_end_idx);
    assert_eq!(props.len(), 1);
    assert_eq!(props[0].value_end_idx - props[0].value_start_idx, 2);
}

#[test]
fn property_integer_value() {
    let (_tokens, ast) = parse_ok(
        r#"
        project myapp {
            optimization: 3;
        }"#,
    );
    let Statement::ProjectDecl(p) = ast.statements[0] else {
        panic!("expected ProjectDecl")
    };
    let props = ast.get_properties(p.property_start_idx, p.property_end_idx);
    assert_eq!(*ast.get_value(props[0].value_start_idx), Value::Integer(3));
}

#[test]
fn property_boolean_value() {
    let (_tokens, ast) = parse_ok(
        r#"
        project myapp {
            werror: true;
        }"#,
    );
    let Statement::ProjectDecl(p) = ast.statements[0] else {
        panic!("expected ProjectDecl")
    };
    let props = ast.get_properties(p.property_start_idx, p.property_end_idx);
    assert_eq!(*ast.get_value(props[0].value_start_idx), Value::Boolean(true));
}

#[test]
fn property_identifier_value() {
    let (_tokens, ast) = parse_ok(
        r#"
        project myapp {
            type: executable;
        }"#,
    );
    let Statement::ProjectDecl(p) = ast.statements[0] else {
        panic!("expected ProjectDecl")
    };
    let props = ast.get_properties(p.property_start_idx, p.property_end_idx);
    assert_eq!(*ast.get_value(props[0].value_start_idx), Value::Identifier("executable"));
}

#[test]
fn property_keyword_as_name() {
    // Keywords should be valid property names
    let (_tokens, ast) = parse_ok(
        r#"
        project myapp {
            target: "something";
        }"#,
    );
    let Statement::ProjectDecl(p) = ast.statements[0] else {
        panic!("expected ProjectDecl")
    };
    let props = ast.get_properties(p.property_start_idx, p.property_end_idx);
    assert_eq!(ast.get_string(props[0].name_idx), "target");
}

//===---------------------------------------------------------------------===//
// Visibility Blocks
//===---------------------------------------------------------------------===//

#[test]
fn visibility_public() {
    let (_tokens, ast) = parse_ok(
        r#"
        target mylib {
            public {
                include_dirs: "include";
            }
        }"#,
    );
    let Statement::TargetDecl(t) = ast.statements[0] else {
        panic!("expected TargetDecl")
    };
    let body = ast.get_statements(t.body_start_idx, t.body_end_idx);
    let Statement::VisibilityBlock(v) = body[0] else {
        panic!("expected VisibilityBlock")
    };
    assert_eq!(v.visibility, Visibility::Public);
}

#[test]
fn visibility_private() {
    let (_tokens, ast) = parse_ok(
        r#"
        target mylib {
            private {
                sources: "src/impl.cpp";
            }
        }"#,
    );
    let Statement::TargetDecl(t) = ast.statements[0] else {
        panic!("expected TargetDecl")
    };
    let body = ast.get_statements(t.body_start_idx, t.body_end_idx);
    let Statement::VisibilityBlock(v) = body[0] else {
        panic!("expected VisibilityBlock")
    };
    assert_eq!(v.visibility, Visibility::Private);
}

#[test]
fn visibility_interface() {
    let (_tokens, ast) = parse_ok(
        r#"
        target mylib {
            interface {
                defines: "MYLIB_INTERFACE";
            }
        }"#,
    );
    let Statement::TargetDecl(t) = ast.statements[0] else {
        panic!("expected TargetDecl")
    };
    let body = ast.get_statements(t.body_start_idx, t.body_end_idx);
    let Statement::VisibilityBlock(v) = body[0] else {
        panic!("expected VisibilityBlock")
    };
    assert_eq!(v.visibility, Visibility::Interface);
}

//===---------------------------------------------------------------------===//
// Expressions
//===---------------------------------------------------------------------===//

#[test]
fn condition_comparison_equal() {
    let (_tokens, ast) = parse_ok(r#"@if platform == "linux" { }"#);
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };
    let Condition::ComparisonExpr(cmp) = if_stmt.condition else {
        panic!("expected ComparisonExpr")
    };
    assert_eq!(cmp.op, Some(ComparisonOperator::Equal));
}

#[test]
fn condition_comparison_not_equal() {
    let (_tokens, ast) = parse_ok(r#"@if arch != "arm" { }"#);
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };
    let Condition::ComparisonExpr(cmp) = if_stmt.condition else {
        panic!("expected ComparisonExpr")
    };
    assert_eq!(cmp.op, Some(ComparisonOperator::NotEqual));
}

#[test]
fn condition_comparison_less() {
    let (_tokens, ast) = parse_ok("@if version < 5 { }");
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };
    let Condition::ComparisonExpr(cmp) = if_stmt.condition else {
        panic!("expected ComparisonExpr")
    };
    assert_eq!(cmp.op, Some(ComparisonOperator::Less));
}

#[test]
fn condition_comparison_greater_equal() {
    let (_tokens, ast) = parse_ok("@if threads >= 4 { }");
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };
    let Condition::ComparisonExpr(cmp) = if_stmt.condition else {
        panic!("expected ComparisonExpr")
    };
    assert_eq!(cmp.op, Some(ComparisonOperator::GreaterEqual));
}

#[test]
fn condition_unary_identifier() {
    let (_tokens, ast) = parse_ok("@if debug { }");
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };
    let Condition::UnaryExpr(unary) = if_stmt.condition else {
        panic!("expected UnaryExpr")
    };
    assert!(!unary.is_negated);
    assert_eq!(unary.operand.kind, OperandType::Value);
}

#[test]
fn condition_not_function_call() {
    let (_tokens, ast) = parse_ok(r#"@if not has_feature("avx2") { }"#);
    let Statement::IfStmt(if_stmt) = ast.statements[0] else {
        panic!("expected IfStmt")
    };
    let Condition::UnaryExpr(unary) = if_stmt.condition else {
        panic!("expected UnaryExpr")
    };
    assert!(unary.is_negated);
    assert_eq!(unary.operand.kind, OperandType::FunctionCall);
}

//===---------------------------------------------------------------------===//
// Error Recovery
//===---------------------------------------------------------------------===//

#[test]
fn error_missing_semicolon() {
    let (_tokens, ast) = parse_err(
        r#"
        project myapp {
            version: "1.0.0"
        }"#,
    );
    assert!(!ast.errors.is_empty());
}

#[test]
fn error_missing_closing_brace() {
    let (_tokens, ast) = parse_err("project myapp {");
    assert!(!ast.errors.is_empty());
}

#[test]
fn error_unexpected_token() {
    let (_tokens, ast) = parse_err("42");
    assert!(!ast.errors.is_empty());
}

#[test]
fn error_recovery_continues_parsing() {
    // Missing semicolon on first property, but second should still parse
    let (_tokens, ast) = parse(
        r#"
        project myapp {
            version: "1.0.0"
            standard: "c++20";
        }"#,
    );
    assert!(!ast.errors.is_empty());
    // Parser should have recovered and parsed the project
    assert_eq!(ast.statements.len(), 1);
}

//===---------------------------------------------------------------------===//
// Multiple Top-Level Statements
//===---------------------------------------------------------------------===//

#[test]
fn multiple_declarations() {
    let (_tokens, ast) = parse_ok(
        r#"
        project myapp { }
        target mylib { }
        dependencies { }"#,
    );
    assert_eq!(ast.statements.len(), 3);
    assert!(matches!(ast.statements[0], Statement::ProjectDecl(_)));
    assert!(matches!(ast.statements[1], Statement::TargetDecl(_)));
    assert!(matches!(ast.statements[2], Statement::DependenciesDecl(_)));
}

#[test]
fn empty_file() {
    let (_tokens, ast) = parse_ok("");
    assert_eq!(ast.statements.len(), 0);
    assert!(ast.errors.is_empty());
}

//===---------------------------------------------------------------------===//
// Complex / Integration
//===---------------------------------------------------------------------===//

#[test]
fn full_project_file() {
    let (_tokens, ast) = parse_ok(
        r#"
        project myapp {
            version: "1.0.0";
            standard: "c++20";
        }

        mixin strict {
            warnings: "all";
            werror: true;
        }

        target mylib with strict {
            type: "static_library";
            public {
                include_dirs: "include";
            }
            private {
                sources: "src/main.cpp", "src/util.cpp";
            }
        }

        dependencies {
            fmt: "10.1.0";
            vulkan?: system;
        }

        @if platform == "linux" {
            defines: "PLATFORM_LINUX";
        } @else {
            defines: "PLATFORM_OTHER";
        }

        "#,
    );
    assert_eq!(ast.statements.len(), 5);
    assert!(matches!(ast.statements[0], Statement::ProjectDecl(_)));
    assert!(matches!(ast.statements[1], Statement::MixinDecl(_)));
    assert!(matches!(ast.statements[2], Statement::TargetDecl(_)));
    assert!(matches!(ast.statements[3], Statement::DependenciesDecl(_)));
    assert!(matches!(ast.statements[4], Statement::IfStmt(_)));
}
