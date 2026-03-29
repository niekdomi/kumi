// Semantic analysis pass over the AST.
//
// Validates scopes, checks for undefined references, detects property conflicts,
// and ensures structural correctness of the build configuration.
//
// The checker runs in two phases:
//   Phase 1 (collect): walk top-level statements and register all named
//                      declarations into the symbol table. Detect duplicates.
//                      Validate option naming (UPPER_SNAKE_CASE, no shadowing).
//   Phase 2 (validate): walk the full AST and validate references,
//                       structural rules, property conflicts, function args,
//                       dependency specs, and conditions.

use crate::diagnostics::Diagnostic;
use crate::lang::ast::{
    Ast, ComparisonExpr, Condition, DependenciesDecl, DependencyValue, Iterable, LoopControl,
    MixinDecl, NodeBase, OperandType, OptionsDecl, Statement, UnaryOperand, Value,
};
use crate::lang::semantic::symbol_table::{DuplicateSymbol, SymbolEntry, SymbolKind, SymbolTable};
use std::collections::HashMap;

//===----------------------------------------------------------------------===//
// Property merge strategies
//===----------------------------------------------------------------------===//

/// Property merge strategy determines how conflicting property values
/// are handled during mixin composition.
#[derive(Clone, Copy, PartialEq, Eq)]
enum MergeStrategy {
    /// Only one value allowed — error on conflict.
    Scalar,
    /// Values are appended from all sources.
    Append,
}

/// Known list properties that use append semantics during mixin composition.
/// Everything not in this list defaults to scalar (error on conflict).
///
/// Sorted alphabetically for binary search.
const LIST_PROPERTIES: &[&str] = &[
    "compile-options",
    "defines",
    "frameworks",
    "headers",
    "include-dirs",
    "link-dirs",
    "link-libraries",
    "link-options",
    "sanitizers",
    "sources",
    "system-include-dirs",
];

#[inline]
fn merge_strategy(name: &str) -> MergeStrategy {
    match LIST_PROPERTIES.binary_search(&name) {
        Ok(_) => MergeStrategy::Append,
        Err(_) => MergeStrategy::Scalar,
    }
}

//===----------------------------------------------------------------------===//
// Context tracking
//===----------------------------------------------------------------------===//

/// Context tracks where we are in the AST tree during validation.
/// Used to enforce structural rules (e.g. visibility blocks only in targets).
#[derive(Clone, Copy, PartialEq, Eq)]
enum Context {
    TopLevel,
    Target,
    Mixin,
}

//===----------------------------------------------------------------------===//
// Builtin registries (see builtins.rs)
//===----------------------------------------------------------------------===//

use super::builtins::{
    builtin_names_list, dep_function_names_list, format_valid_values, is_builtin_variable,
    lookup_builtin, lookup_builtin_variable, lookup_dep_function,
};

//===----------------------------------------------------------------------===//
// UPPER_SNAKE_CASE validation
//===----------------------------------------------------------------------===//

/// Check if a name is valid `UPPER_SNAKE_CASE`: A-Z, 0-9, _ only,
/// must start with A-Z or _, no leading/trailing/double underscores.
#[inline]
fn is_upper_snake_case(name: &str) -> bool {
    let bytes = name.as_bytes();
    if bytes.is_empty() {
        return false;
    }
    for &b in bytes {
        if !matches!(b, b'A'..=b'Z' | b'0'..=b'9' | b'_') {
            return false;
        }
    }
    true
}

/// Convert an identifier to `UPPER_SNAKE_CASE` for the help suggestion.
fn to_upper_snake_case(name: &str) -> String {
    let mut result = String::with_capacity(name.len() + 4);
    let bytes = name.as_bytes();
    for (i, &b) in bytes.iter().enumerate() {
        if b.is_ascii_uppercase() && i > 0 {
            let prev = bytes[i - 1];
            // Insert _ before uppercase if preceded by lowercase or digit
            if prev.is_ascii_lowercase() || prev.is_ascii_digit() {
                result.push('_');
            }
        }
        if b == b'-' {
            result.push('_');
        } else {
            result.push(b.to_ascii_uppercase() as char);
        }
    }
    result
}

//===----------------------------------------------------------------------===//
// Checker
//===----------------------------------------------------------------------===//

pub struct Checker<'a> {
    symbols: SymbolTable<'a>,
    errors: Vec<Diagnostic>,
    file_idx: u16,
}

impl Default for Checker<'_> {
    fn default() -> Self {
        Self::new()
    }
}

impl<'a> Checker<'a> {
    pub fn new() -> Self {
        Self {
            symbols: SymbolTable::new(),
            errors: Vec::new(),
            file_idx: 0,
        }
    }

    /// Run semantic analysis on a single AST. Returns accumulated diagnostics.
    pub fn check(mut self, ast: &Ast<'a>) -> Vec<Diagnostic> {
        self.collect_declarations(ast);
        self.validate(ast);
        self.errors
    }

    //===------------------------------------------------------------------===//
    // Phase 1: Collect declarations
    //===------------------------------------------------------------------===//

    fn collect_declarations(&mut self, ast: &Ast<'a>) {
        let mut project_count = 0u32;
        let mut project_pos = 0u32;

        // Track merged dependencies for cross-block duplicate detection
        let mut seen_deps: HashMap<&str, (u32, &str)> = HashMap::new();

        for stmt in &ast.statements {
            match stmt {
                Statement::TargetDecl(decl) => {
                    self.register_symbol(ast, decl.name_idx, decl.base, SymbolKind::Target);
                }
                Statement::MixinDecl(decl) => {
                    self.register_symbol(ast, decl.name_idx, decl.base, SymbolKind::Mixin);
                }
                Statement::ProfileDecl(decl) => {
                    self.register_symbol(ast, decl.name_idx, decl.base, SymbolKind::Profile);
                }
                Statement::OptionsDecl(decl) => {
                    // Options blocks merge — register each option individually
                    for opt in ast.get_options(decl.option_start_idx, decl.option_end_idx) {
                        let name = ast.get_string(opt.name_idx);
                        self.validate_option_name(name, opt.base);
                        self.register_symbol(ast, opt.name_idx, opt.base, SymbolKind::Option);
                    }
                }
                Statement::ProjectDecl(decl) => {
                    project_count += 1;
                    if project_count == 1 {
                        project_pos = decl.base.start_idx;
                    } else {
                        self.errors.push(Diagnostic::new(
                            "duplicate project declaration",
                            decl.base.start_idx,
                            format!("first project declaration at offset {project_pos}"),
                        ));
                    }
                }
                Statement::DependenciesDecl(decl) => {
                    // Dependencies blocks merge — check for cross-block duplicates
                    let deps = ast.get_dependencies(decl.dep_start_idx, decl.dep_end_idx);
                    for dep in deps {
                        let name = ast.get_string(dep.name_idx);
                        if let Some(&(first_pos, first_ver)) = seen_deps.get(name) {
                            // Get current version string for comparison
                            let cur_ver = match &dep.value {
                                DependencyValue::String(s) => *s,
                                _ => "",
                            };
                            if cur_ver == first_ver && !cur_ver.is_empty() {
                                self.errors.push(Diagnostic::new(
                                    format!("dependency '{name}' declared with same version"),
                                    dep.base.start_idx,
                                    format!("first declared at offset {first_pos}"),
                                ));
                            } else {
                                self.errors.push(Diagnostic::new(
                                    format!("duplicate dependency '{name}'"),
                                    dep.base.start_idx,
                                    format!("first declared at offset {first_pos}"),
                                ));
                            }
                        } else {
                            let ver = match &dep.value {
                                DependencyValue::String(s) => *s,
                                _ => "",
                            };
                            seen_deps.insert(name, (dep.base.start_idx, ver));
                        }
                    }
                }
                // Recurse into @if/@for to find nested declarations
                Statement::IfStmt(stmt) => {
                    self.collect_in_block(ast, stmt.then_start_idx, stmt.then_end_idx);
                    self.collect_in_block(ast, stmt.else_start_idx, stmt.else_end_idx);
                }
                Statement::ForStmt(stmt) => {
                    self.collect_in_block(ast, stmt.body_start_idx, stmt.body_end_idx);
                }
                _ => {}
            }
        }
    }

    /// Collect declarations from a nested statement block (e.g. inside @if/@for).
    fn collect_in_block(&mut self, ast: &Ast<'a>, start: u32, end: u32) {
        for stmt in ast.get_statements(start, end) {
            match stmt {
                Statement::TargetDecl(decl) => {
                    self.register_symbol(ast, decl.name_idx, decl.base, SymbolKind::Target);
                }
                Statement::MixinDecl(decl) => {
                    self.register_symbol(ast, decl.name_idx, decl.base, SymbolKind::Mixin);
                }
                Statement::ProfileDecl(decl) => {
                    self.register_symbol(ast, decl.name_idx, decl.base, SymbolKind::Profile);
                }
                Statement::IfStmt(stmt) => {
                    self.collect_in_block(ast, stmt.then_start_idx, stmt.then_end_idx);
                    self.collect_in_block(ast, stmt.else_start_idx, stmt.else_end_idx);
                }
                Statement::ForStmt(stmt) => {
                    self.collect_in_block(ast, stmt.body_start_idx, stmt.body_end_idx);
                }
                _ => {}
            }
        }
    }

    fn register_symbol(&mut self, ast: &Ast<'a>, name_idx: u32, base: NodeBase, kind: SymbolKind) {
        let name = ast.get_string(name_idx);
        let entry = SymbolEntry {
            name_idx,
            position: base.start_idx,
            kind,
            file_idx: self.file_idx,
        };
        if let Err(DuplicateSymbol { existing, .. }) = self.symbols.register(name, entry) {
            let kind_str = match kind {
                SymbolKind::Target => "target",
                SymbolKind::Mixin => "mixin",
                SymbolKind::Profile => "profile",
                SymbolKind::Option => "option",
            };
            self.errors.push(Diagnostic::new(
                format!("duplicate {kind_str} definition '{name}'"),
                base.start_idx,
                format!("first defined at offset {}", existing.position),
            ));
        }
    }

    /// Validate option name: must be `UPPER_SNAKE_CASE`, must not shadow builtins.
    fn validate_option_name(&mut self, name: &str, base: NodeBase) {
        if !is_upper_snake_case(name) {
            let suggestion = to_upper_snake_case(name);
            self.errors.push(Diagnostic::new(
                format!("option name '{name}' must be UPPER_SNAKE_CASE"),
                base.start_idx,
                format!("help: rename to '{suggestion}'"),
            ));
        }
        if is_builtin_variable(name.to_ascii_lowercase().as_str()) || is_builtin_variable(name) {
            self.errors.push(Diagnostic::new(
                format!("option name '{name}' shadows a builtin variable"),
                base.start_idx,
                "",
            ));
        }
    }

    //===------------------------------------------------------------------===//
    // Phase 2: Validate
    //===------------------------------------------------------------------===//

    fn validate(&mut self, ast: &Ast<'a>) {
        let loop_vars: Vec<&'a str> = Vec::new();
        for stmt in &ast.statements {
            self.validate_statement(ast, stmt, Context::TopLevel, &loop_vars);
        }
    }

    fn validate_statement(
        &mut self,
        ast: &Ast<'a>,
        stmt: &Statement,
        ctx: Context,
        loop_vars: &[&'a str],
    ) {
        match stmt {
            Statement::TargetDecl(decl) => {
                if ctx != Context::TopLevel {
                    self.errors.push(Diagnostic::new(
                        "target declarations are only allowed at the top level",
                        decl.base.start_idx,
                        "",
                    ));
                }
                self.validate_mixin_refs(ast, decl.mixin_start_idx, decl.mixin_end_idx, decl.base);
                self.validate_block(
                    ast,
                    decl.body_start_idx,
                    decl.body_end_idx,
                    Context::Target,
                    loop_vars,
                );
            }
            Statement::MixinDecl(decl) => {
                if ctx != Context::TopLevel {
                    self.errors.push(Diagnostic::new(
                        "mixin declarations are only allowed at the top level",
                        decl.base.start_idx,
                        "",
                    ));
                }
                self.validate_block(
                    ast,
                    decl.body_start_idx,
                    decl.body_end_idx,
                    Context::Mixin,
                    loop_vars,
                );
            }
            Statement::ProfileDecl(decl) => {
                if ctx != Context::TopLevel {
                    self.errors.push(Diagnostic::new(
                        "profile declarations are only allowed at the top level",
                        decl.base.start_idx,
                        "",
                    ));
                }
                self.validate_mixin_refs(ast, decl.mixin_start_idx, decl.mixin_end_idx, decl.base);
                self.validate_properties(ast, decl.property_start_idx, decl.property_end_idx);
            }
            Statement::ProjectDecl(decl) => {
                if ctx != Context::TopLevel {
                    self.errors.push(Diagnostic::new(
                        "project declarations are only allowed at the top level",
                        decl.base.start_idx,
                        "",
                    ));
                }
                self.validate_properties(ast, decl.property_start_idx, decl.property_end_idx);
            }
            Statement::WorkspaceDecl(decl) => {
                if ctx != Context::TopLevel {
                    self.errors.push(Diagnostic::new(
                        "workspace declarations are only allowed at the top level",
                        decl.base.start_idx,
                        "",
                    ));
                }
                self.validate_properties(ast, decl.property_start_idx, decl.property_end_idx);
            }
            Statement::DependenciesDecl(decl) => {
                if ctx != Context::TopLevel {
                    self.errors.push(Diagnostic::new(
                        "dependencies declarations are only allowed at the top level",
                        decl.base.start_idx,
                        "",
                    ));
                }
                self.validate_dependencies(ast, decl);
            }
            Statement::OptionsDecl(decl) => {
                if ctx != Context::TopLevel {
                    self.errors.push(Diagnostic::new(
                        "options declarations are only allowed at the top level",
                        decl.base.start_idx,
                        "",
                    ));
                }
                self.validate_options(ast, decl);
            }
            Statement::InstallDecl(decl) => {
                if ctx != Context::TopLevel {
                    self.errors.push(Diagnostic::new(
                        "install declarations are only allowed at the top level",
                        decl.base.start_idx,
                        "",
                    ));
                }
                self.validate_properties(ast, decl.property_start_idx, decl.property_end_idx);
            }
            Statement::PackageDecl(decl) => {
                if ctx != Context::TopLevel {
                    self.errors.push(Diagnostic::new(
                        "package declarations are only allowed at the top level",
                        decl.base.start_idx,
                        "",
                    ));
                }
                self.validate_properties(ast, decl.property_start_idx, decl.property_end_idx);
            }
            Statement::ScriptDecl(decl) => {
                if ctx != Context::TopLevel {
                    self.errors.push(Diagnostic::new(
                        "script declarations are only allowed at the top level",
                        decl.base.start_idx,
                        "",
                    ));
                }
                self.validate_properties(ast, decl.property_start_idx, decl.property_end_idx);
            }
            Statement::VisibilityBlock(block) => {
                if ctx != Context::Target && ctx != Context::Mixin {
                    self.errors.push(Diagnostic::new(
                        "visibility blocks are only allowed inside target or mixin declarations",
                        block.base.start_idx,
                        "",
                    ));
                }
                self.validate_properties(ast, block.property_start_idx, block.property_end_idx);
            }
            Statement::IfStmt(stmt) => {
                self.validate_condition(ast, &stmt.condition);
                self.validate_block(ast, stmt.then_start_idx, stmt.then_end_idx, ctx, loop_vars);
                self.validate_block(ast, stmt.else_start_idx, stmt.else_end_idx, ctx, loop_vars);
            }
            Statement::ForStmt(stmt) => {
                self.validate_iterable(ast, &stmt.iterable);
                let var_name = ast.get_string(stmt.variable_name_idx);
                if loop_vars.contains(&var_name) {
                    self.errors.push(Diagnostic::new(
                        format!("@for variable '{var_name}' shadows an outer loop variable"),
                        stmt.base.start_idx,
                        "consider using a different variable name",
                    ));
                }
                let mut inner_vars = loop_vars.to_vec();
                inner_vars.push(var_name);
                self.validate_block(ast, stmt.body_start_idx, stmt.body_end_idx, ctx, &inner_vars);
            }
            Statement::LoopControlStmt(stmt) => {
                if loop_vars.is_empty() {
                    let keyword = match stmt.control {
                        LoopControl::Break => "@break",
                        LoopControl::Continue => "@continue",
                    };
                    self.errors.push(Diagnostic::new(
                        format!("{keyword} is only allowed inside @for loops"),
                        stmt.base.start_idx,
                        "",
                    ));
                }
            }
            Statement::DiagnosticStmt(_) => {}
            Statement::Property(_) => {
                // Bare properties at top-level are unusual but not invalid
                // (they become project-level settings).
            }
        }
    }

    fn validate_block(
        &mut self,
        ast: &Ast<'a>,
        start: u32,
        end: u32,
        ctx: Context,
        loop_vars: &[&'a str],
    ) {
        // The SoA layout stores nested block contents before their parent
        // statements in all_statements. When iterating [start, end), we must
        // skip entries that belong to a nested sub-block to avoid double-processing.
        let stmts = ast.get_statements(start, end);
        let mut nested_ranges: Vec<(u32, u32)> = Vec::new();
        for stmt in stmts {
            match stmt {
                Statement::IfStmt(s) => {
                    if s.then_start_idx < s.then_end_idx {
                        nested_ranges.push((s.then_start_idx, s.then_end_idx));
                    }
                    if s.else_start_idx < s.else_end_idx {
                        nested_ranges.push((s.else_start_idx, s.else_end_idx));
                    }
                }
                Statement::ForStmt(s) => {
                    if s.body_start_idx < s.body_end_idx {
                        nested_ranges.push((s.body_start_idx, s.body_end_idx));
                    }
                }
                Statement::TargetDecl(s) => {
                    if s.body_start_idx < s.body_end_idx {
                        nested_ranges.push((s.body_start_idx, s.body_end_idx));
                    }
                }
                Statement::MixinDecl(s) => {
                    if s.body_start_idx < s.body_end_idx {
                        nested_ranges.push((s.body_start_idx, s.body_end_idx));
                    }
                }
                _ => {}
            }
        }

        // Detect unreachable code after @break/@continue
        let mut found_loop_control = false;
        let mut control_pos = 0u32;

        // Second pass: validate only direct children (not in a nested range).
        for (i, stmt) in stmts.iter().enumerate() {
            let abs_idx = start + i as u32;
            let is_nested = nested_ranges.iter().any(|&(ns, ne)| abs_idx >= ns && abs_idx < ne);
            if is_nested {
                continue;
            }

            if found_loop_control {
                // Get position of unreachable statement
                let pos = match stmt {
                    Statement::Property(p) => p.base.start_idx,
                    Statement::TargetDecl(d) => d.base.start_idx,
                    Statement::MixinDecl(d) => d.base.start_idx,
                    Statement::ProfileDecl(d) => d.base.start_idx,
                    Statement::ProjectDecl(d) => d.base.start_idx,
                    Statement::WorkspaceDecl(d) => d.base.start_idx,
                    Statement::DependenciesDecl(d) => d.base.start_idx,
                    Statement::OptionsDecl(d) => d.base.start_idx,
                    Statement::InstallDecl(d) => d.base.start_idx,
                    Statement::PackageDecl(d) => d.base.start_idx,
                    Statement::ScriptDecl(d) => d.base.start_idx,
                    Statement::VisibilityBlock(b) => b.base.start_idx,
                    Statement::IfStmt(s) => s.base.start_idx,
                    Statement::ForStmt(s) => s.base.start_idx,
                    Statement::LoopControlStmt(s) => s.base.start_idx,
                    Statement::DiagnosticStmt(d) => d.base.start_idx,
                };
                self.errors.push(Diagnostic::new(
                    "unreachable code",
                    pos,
                    format!(
                        "code after @break/@continue at offset {control_pos} is never executed"
                    ),
                ));
                // Only warn once per block
                break;
            }

            if let Statement::LoopControlStmt(lc) = stmt
                && !loop_vars.is_empty()
            {
                found_loop_control = true;
                control_pos = lc.base.start_idx;
            }

            self.validate_statement(ast, stmt, ctx, loop_vars);
        }
    }

    //===------------------------------------------------------------------===//
    // Mixin reference validation
    //===------------------------------------------------------------------===//

    fn validate_mixin_refs(&mut self, ast: &Ast<'a>, start: u32, end: u32, decl_base: NodeBase) {
        // Check for duplicate mixins in the with list.
        let mut seen: Vec<&str> = Vec::new();
        for i in start..end {
            let mixin_name = ast.get_string(i);
            if seen.contains(&mixin_name) {
                self.errors.push(Diagnostic::new(
                    format!("duplicate mixin '{mixin_name}' in with list"),
                    decl_base.start_idx,
                    "",
                ));
            } else {
                seen.push(mixin_name);
                if self.symbols.lookup(mixin_name, SymbolKind::Mixin).is_none() {
                    self.errors.push(Diagnostic::new(
                        format!("undefined mixin '{mixin_name}'"),
                        decl_base.start_idx,
                        self.suggest_similar(mixin_name, SymbolKind::Mixin),
                    ));
                }
            }
        }

        // Check for scalar property conflicts between composed mixins.
        self.check_mixin_property_conflicts(ast, start, end, decl_base);
    }

    //===------------------------------------------------------------------===//
    // Mixin property conflict detection
    //===------------------------------------------------------------------===//

    fn check_mixin_property_conflicts(
        &mut self,
        ast: &Ast<'a>,
        mixin_start: u32,
        mixin_end: u32,
        decl_base: NodeBase,
    ) {
        if mixin_start == mixin_end {
            return;
        }

        // Collect scalar properties from each mixin.
        // Key: property name, Value: (value position, mixin name)
        let mut scalar_props: HashMap<&'a str, (u32, &'a str)> = HashMap::new();

        for i in mixin_start..mixin_end {
            let mixin_name = ast.get_string(i);
            if let Some(entry) = self.symbols.lookup(mixin_name, SymbolKind::Mixin) {
                let mixin_decl = self.find_mixin_decl(ast, entry.name_idx);
                if let Some(decl) = mixin_decl {
                    self.collect_scalar_props_from_block(
                        ast,
                        decl.body_start_idx,
                        decl.body_end_idx,
                        mixin_name,
                        &mut scalar_props,
                        decl_base,
                    );
                }
            }
        }
    }

    fn collect_scalar_props_from_block(
        &mut self,
        ast: &Ast<'a>,
        start: u32,
        end: u32,
        mixin_name: &'a str,
        seen: &mut HashMap<&'a str, (u32, &'a str)>,
        decl_base: NodeBase,
    ) {
        for stmt in ast.get_statements(start, end) {
            match stmt {
                Statement::Property(prop) => {
                    let prop_name = ast.get_string(prop.name_idx);
                    if merge_strategy(prop_name) == MergeStrategy::Scalar {
                        if let Some(&(_, first_mixin)) = seen.get(prop_name) {
                            if first_mixin != mixin_name {
                                self.errors.push(Diagnostic::new(
                                    format!(
                                        "scalar property '{prop_name}' has conflicting values in mixin composition"
                                    ),
                                    decl_base.start_idx,
                                    format!(
                                        "set in mixin '{first_mixin}' and mixin '{mixin_name}'"
                                    ),
                                ));
                            }
                        } else {
                            seen.insert(prop_name, (prop.base.start_idx, mixin_name));
                        }
                    }
                }
                Statement::VisibilityBlock(block) => {
                    self.collect_scalar_props_from_properties(
                        ast,
                        block.property_start_idx,
                        block.property_end_idx,
                        mixin_name,
                        seen,
                        decl_base,
                    );
                }
                _ => {}
            }
        }
    }

    fn collect_scalar_props_from_properties(
        &mut self,
        ast: &Ast<'a>,
        start: u32,
        end: u32,
        mixin_name: &'a str,
        seen: &mut HashMap<&'a str, (u32, &'a str)>,
        decl_base: NodeBase,
    ) {
        for prop in ast.get_properties(start, end) {
            let prop_name = ast.get_string(prop.name_idx);
            if merge_strategy(prop_name) == MergeStrategy::Scalar {
                if let Some(&(_, first_mixin)) = seen.get(prop_name) {
                    if first_mixin != mixin_name {
                        self.errors.push(Diagnostic::new(
                            format!(
                                "scalar property '{prop_name}' has conflicting values in mixin composition"
                            ),
                            decl_base.start_idx,
                            format!("set in mixin '{first_mixin}' and mixin '{mixin_name}'"),
                        ));
                    }
                } else {
                    seen.insert(prop_name, (prop.base.start_idx, mixin_name));
                }
            }
        }
    }

    /// Find the `MixinDecl` in the AST by its name index.
    fn find_mixin_decl(&self, ast: &Ast<'a>, name_idx: u32) -> Option<MixinDecl> {
        for stmt in &ast.statements {
            if let Statement::MixinDecl(decl) = stmt
                && decl.name_idx == name_idx
            {
                return Some(*decl);
            }
        }
        for stmt in &ast.all_statements {
            if let Statement::MixinDecl(decl) = stmt
                && decl.name_idx == name_idx
            {
                return Some(*decl);
            }
        }
        None
    }

    //===------------------------------------------------------------------===//
    // Property validation
    //===------------------------------------------------------------------===//

    fn validate_properties(&mut self, ast: &Ast<'a>, start: u32, end: u32) {
        // Check for duplicate scalar properties within the same block.
        let mut seen_scalars: HashMap<&str, u32> = HashMap::new();

        for prop in ast.get_properties(start, end) {
            let name = ast.get_string(prop.name_idx);
            if merge_strategy(name) == MergeStrategy::Scalar {
                if let Some(&first_pos) = seen_scalars.get(name) {
                    self.errors.push(Diagnostic::new(
                        format!("duplicate property '{name}'"),
                        prop.base.start_idx,
                        format!("first set at offset {first_pos}"),
                    ));
                } else {
                    seen_scalars.insert(name, prop.base.start_idx);
                }
            }
        }
    }

    //===------------------------------------------------------------------===//
    // Dependencies validation
    //===------------------------------------------------------------------===//

    fn validate_dependencies(&mut self, ast: &Ast<'a>, decl: &DependenciesDecl) {
        let deps = ast.get_dependencies(decl.dep_start_idx, decl.dep_end_idx);

        for dep in deps {
            // Validate function calls in dependency values.
            if let DependencyValue::FunctionCall(func) = &dep.value {
                let func_name = ast.get_string(func.name_idx);
                match lookup_dep_function(func_name) {
                    Some(expected) => {
                        let actual = (func.arg_end_idx - func.arg_start_idx) as usize;
                        if actual != expected {
                            self.errors.push(Diagnostic::new(
                                format!(
                                    "function '{}' expects {} argument{}, found {}",
                                    func_name,
                                    expected,
                                    if expected == 1 { "" } else { "s" },
                                    actual
                                ),
                                func.base.start_idx,
                                "",
                            ));
                        }
                    }
                    None => {
                        self.errors.push(Diagnostic::new(
                            format!("unknown dependency function '{func_name}'"),
                            func.base.start_idx,
                            format!("available: {}", dep_function_names_list()),
                        ));
                    }
                }
            }

            // Validate inline option properties for duplicates.
            if dep.option_start_idx < dep.option_end_idx {
                self.validate_properties(ast, dep.option_start_idx, dep.option_end_idx);
            }
        }
    }

    //===------------------------------------------------------------------===//
    // Options validation
    //===------------------------------------------------------------------===//

    fn validate_options(&mut self, ast: &Ast<'a>, decl: &OptionsDecl) {
        for opt in ast.get_options(decl.option_start_idx, decl.option_end_idx) {
            // Validate constraint properties for duplicates.
            if opt.constraint_start_idx < opt.constraint_end_idx {
                self.validate_properties(ast, opt.constraint_start_idx, opt.constraint_end_idx);
            }
        }
    }

    //===------------------------------------------------------------------===//
    // Condition and iterable validation
    //===------------------------------------------------------------------===//

    fn validate_condition(&mut self, ast: &Ast<'a>, condition: &Condition) {
        match condition {
            Condition::LogicalExpr(expr) => {
                for cmp in &ast.all_comparison_exprs
                    [expr.operand_start_idx as usize..expr.operand_end_idx as usize]
                {
                    self.validate_comparison_expr(ast, cmp);
                }
            }
            Condition::ComparisonExpr(expr) => {
                self.validate_comparison_expr(ast, expr);
            }
            Condition::UnaryExpr(expr) => {
                self.validate_unary_operand(ast, &expr.operand);
            }
        }
    }

    fn validate_comparison_expr(&mut self, ast: &Ast<'a>, expr: &ComparisonExpr) {
        let left = &ast.all_unary_exprs[expr.left_idx as usize];
        self.validate_unary_operand(ast, &left.operand);

        if let Some(right_idx) = expr.right_idx {
            let right = &ast.all_unary_exprs[right_idx as usize];
            self.validate_unary_operand(ast, &right.operand);

            // If the left side is a builtin function/variable with known valid values,
            // check that the right side is in the allowed set.
            let valid_values = self.get_valid_values_for_operand(ast, &left.operand);
            if !valid_values.is_empty()
                && let Some(rhs_str) = self.extract_string_value(ast, &right.operand)
            {
                // Strip quotes from the string literal
                let rhs_clean = rhs_str.trim_matches('"');
                if !valid_values.contains(&rhs_clean) {
                    let pos = self.get_operand_position(ast, &right.operand);
                    self.errors.push(Diagnostic::new(
                        format!("invalid value \"{rhs_clean}\""),
                        pos,
                        format!("valid values: {}", format_valid_values(valid_values)),
                    ));
                }
            }

            // Also check left against right's valid values (e.g., "linux" == platform())
            let right_valid = self.get_valid_values_for_operand(ast, &right.operand);
            if !right_valid.is_empty()
                && let Some(lhs_str) = self.extract_string_value(ast, &left.operand)
            {
                let lhs_clean = lhs_str.trim_matches('"');
                if !right_valid.contains(&lhs_clean) {
                    let pos = self.get_operand_position(ast, &left.operand);
                    self.errors.push(Diagnostic::new(
                        format!("invalid value \"{lhs_clean}\""),
                        pos,
                        format!("valid values: {}", format_valid_values(right_valid)),
                    ));
                }
            }
        }
    }

    /// Get the valid values for an operand if it's a builtin function or variable.
    fn get_valid_values_for_operand(
        &self,
        ast: &Ast<'a>,
        operand: &UnaryOperand,
    ) -> &'static [&'static str] {
        match operand.kind {
            OperandType::FunctionCall => {
                let func = &ast.all_function_calls[operand.idx as usize];
                let name = ast.get_string(func.name_idx);
                if let Some(builtin) = lookup_builtin(name) {
                    return builtin.valid_values;
                }
            }
            OperandType::Value => {
                let val = &ast.all_values[operand.idx as usize];
                if let Value::Identifier(name) = val
                    && let Some(var) = lookup_builtin_variable(name)
                {
                    return var.valid_values;
                }
            }
            _ => {}
        }
        &[]
    }

    /// Extract a string literal from an operand (for value validation).
    fn extract_string_value<'b>(&self, ast: &'b Ast<'a>, operand: &UnaryOperand) -> Option<&'b str>
    where
        'a: 'b,
    {
        if operand.kind == OperandType::Value {
            let val = &ast.all_values[operand.idx as usize];
            match val {
                Value::String(s) => return Some(s),
                Value::Identifier(s) => return Some(s),
                _ => {}
            }
        }
        None
    }

    /// Get the source position of an operand for error reporting.
    fn get_operand_position(&self, ast: &Ast<'a>, operand: &UnaryOperand) -> u32 {
        match operand.kind {
            OperandType::FunctionCall => {
                ast.all_function_calls[operand.idx as usize].base.start_idx
            }
            _ => 0,
        }
    }

    fn validate_unary_operand(&mut self, ast: &Ast<'a>, operand: &UnaryOperand) {
        match operand.kind {
            OperandType::FunctionCall => {
                let func = &ast.all_function_calls[operand.idx as usize];
                let name = ast.get_string(func.name_idx);
                match lookup_builtin(name) {
                    Some(builtin) => {
                        let actual = (func.arg_end_idx - func.arg_start_idx) as usize;
                        if actual != builtin.arg_count {
                            self.errors.push(Diagnostic::new(
                                format!(
                                    "function '{}' expects {} argument{}, found {}",
                                    name,
                                    builtin.arg_count,
                                    if builtin.arg_count == 1 { "" } else { "s" },
                                    actual
                                ),
                                func.base.start_idx,
                                "",
                            ));
                        }
                    }
                    None => {
                        self.errors.push(Diagnostic::new(
                            format!("unknown function '{name}'"),
                            func.base.start_idx,
                            format!("available functions: {}", builtin_names_list()),
                        ));
                    }
                }
            }
            OperandType::Value | OperandType::LogicalExpr => {}
        }
    }

    fn validate_iterable(&mut self, ast: &Ast<'a>, iterable: &Iterable) {
        if let Iterable::FunctionCall(func) = iterable {
            let name = ast.get_string(func.name_idx);
            match lookup_builtin(name) {
                Some(builtin) => {
                    let actual = (func.arg_end_idx - func.arg_start_idx) as usize;
                    if actual != builtin.arg_count {
                        self.errors.push(Diagnostic::new(
                            format!(
                                "function '{}' expects {} argument{}, found {}",
                                name,
                                builtin.arg_count,
                                if builtin.arg_count == 1 { "" } else { "s" },
                                actual
                            ),
                            func.base.start_idx,
                            "",
                        ));
                    }
                }
                None => {
                    self.errors.push(Diagnostic::new(
                        format!("unknown function '{name}'"),
                        func.base.start_idx,
                        format!("available functions: {}", builtin_names_list()),
                    ));
                }
            }
        }
    }

    //===------------------------------------------------------------------===//
    // Helpers
    //===------------------------------------------------------------------===//

    /// Produce a "did you mean 'X'?" suggestion using simple edit-distance.
    fn suggest_similar(&self, name: &str, kind: SymbolKind) -> String {
        let mut best: Option<(&str, usize)> = None;
        for candidate in self.symbols.names(kind) {
            let dist = Self::edit_distance(name, candidate);
            if dist <= 3 && (best.is_none() || dist < best.unwrap().1) {
                best = Some((candidate, dist));
            }
        }

        let mut available: Vec<&str> = self.symbols.names(kind).collect();
        available.sort_unstable();

        if let Some((suggestion, _)) = best {
            format!("did you mean '{suggestion}'?")
        } else if !available.is_empty() {
            format!("available: {}", available.join(", "))
        } else {
            String::new()
        }
    }

    /// Simple Levenshtein edit distance for short strings.
    fn edit_distance(a: &str, b: &str) -> usize {
        let a = a.as_bytes();
        let b = b.as_bytes();
        let mut prev: Vec<usize> = (0..=b.len()).collect();
        let mut curr = vec![0; b.len() + 1];

        for (i, &ca) in a.iter().enumerate() {
            curr[0] = i + 1;
            for (j, &cb) in b.iter().enumerate() {
                let cost = usize::from(ca != cb);
                curr[j + 1] = (prev[j] + cost).min(prev[j + 1] + 1).min(curr[j] + 1);
            }
            std::mem::swap(&mut prev, &mut curr);
        }
        prev[b.len()]
    }
}
