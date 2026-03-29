use crate::diagnostics::Diagnostic;
use crate::lang::ast::{
    Ast, ComparisonExpr, ComparisonOperator, Condition, DependenciesDecl, DependencySpec,
    DependencyValue, DiagnosticLevel, DiagnosticStmt, ForStmt, FunctionCall, IfStmt, InstallDecl,
    Iterable, List, LogicalExpr, LogicalOperator, LoopControl, LoopControlStmt, MixinDecl,
    NodeBase, OperandType, OptionSpec, OptionsDecl, PackageDecl, ProfileDecl, ProjectDecl,
    Property, Range, ScriptDecl, Statement, TargetDecl, UnaryExpr, UnaryOperand, Value, Visibility,
    VisibilityBlock, WorkspaceDecl,
};
use crate::lang::lex::{Token, TokenType};

pub struct Parser<'a> {
    tokens: &'a [Token],
    position: usize,
    source: &'a [u8],
}

impl<'a> Parser<'a> {
    pub const fn new(tokens: &'a [Token], source: &'a [u8]) -> Self {
        Self {
            tokens,
            position: 0,
            source,
        }
    }

    pub fn parse(mut self, file_path: &'a str) -> Ast<'a> {
        let mut ast = Ast::new(file_path);

        while self.peek(0).kind != TokenType::EndOfFile {
            match self.parse_statement(&mut ast) {
                Ok(stmt) => ast.statements.push(stmt),
                Err(err) => {
                    ast.errors.push(err);
                    self.synchronize();
                }
            }
        }

        ast
    }

    #[inline(always)]
    fn synchronize(&mut self) {
        if self.peek(0).kind == TokenType::EndOfFile {
            return;
        }
        self.advance();
        while self.peek(0).kind != TokenType::EndOfFile {
            if self.tokens[self.position - 1].kind == TokenType::Semicolon {
                return;
            }
            match self.peek(0).kind {
                TokenType::Project
                | TokenType::Workspace
                | TokenType::Target
                | TokenType::Dependencies
                | TokenType::Options
                | TokenType::Mixin
                | TokenType::Profile
                | TokenType::Install
                | TokenType::Package
                | TokenType::Script
                | TokenType::AtIf
                | TokenType::AtFor
                | TokenType::AtBreak
                | TokenType::AtContinue
                | TokenType::AtError
                | TokenType::AtWarning
                | TokenType::AtInfo
                | TokenType::AtDebug
                | TokenType::Public
                | TokenType::Private
                | TokenType::Interface => {
                    return;
                }
                _ => {}
            }
            self.advance();
        }
    }

    #[inline(always)]
    fn advance(&mut self) -> &'a Token {
        let token = &self.tokens[self.position];
        self.position += 1;
        token
    }

    #[inline(always)]
    fn expect(&mut self, kind: TokenType) -> Result<&'a Token, Diagnostic> {
        if self.peek(0).kind != kind {
            let peek_token = self.peek(0);

            let error_pos = if kind == TokenType::Semicolon && self.position > 0 {
                let prev_token = &self.tokens[self.position - 1];
                prev_token.position + prev_token.length
            } else {
                peek_token.position
            };

            let expected_str = match kind {
                TokenType::LeftBrace => "{",
                TokenType::RightBrace => "}",
                TokenType::LeftBracket => "[",
                TokenType::RightBracket => "]",
                TokenType::LeftParen => "(",
                TokenType::RightParen => ")",
                TokenType::Semicolon => ";",
                TokenType::Colon => ":",
                TokenType::Comma => ",",
                TokenType::Identifier => "identifier",
                TokenType::String => "string",
                TokenType::Number => "number",
                _ => {
                    return Err(Diagnostic::new(
                        format!("expected '{:?}', got '{}'", kind, self.get_string(peek_token)),
                        error_pos,
                        "",
                    ));
                }
            };

            let value = self.get_string(peek_token);
            return Err(Diagnostic::new(
                format!("expected {expected_str}, got {value}"),
                error_pos,
                "",
            ));
        }
        Ok(self.advance())
    }

    #[inline(always)]
    fn match_token(&mut self, kind: TokenType) -> bool {
        if self.peek(0).kind == kind {
            self.advance();
            true
        } else {
            false
        }
    }

    #[inline(always)]
    fn peek(&self, k: usize) -> &'a Token {
        let pos = self.position + k;
        if pos >= self.tokens.len() {
            &self.tokens[self.tokens.len() - 1]
        } else {
            &self.tokens[pos]
        }
    }

    #[inline(always)]
    fn expect_identifier_or_keyword(&mut self) -> Result<&'a Token, Diagnostic> {
        let kind = self.peek(0).kind;
        if kind == TokenType::Identifier || kind.is_keyword() {
            Ok(self.advance())
        } else {
            let token = self.peek(0);
            Err(Diagnostic::new(
                format!("expected identifier or keyword, got '{}'", self.get_string(token)),
                token.position,
                "identifiers must start with a letter or underscore, followed by letters or digits",
            ))
        }
    }

    /// Returns the end byte offset (exclusive) of a token.
    #[inline(always)]
    const fn end_of(&self, token: &Token) -> u32 {
        token.position + token.length
    }

    /// Returns the end byte offset of the previously consumed token.
    #[inline(always)]
    fn prev_end(&self) -> u32 {
        let prev = &self.tokens[self.position - 1];
        prev.position + prev.length
    }

    #[inline(always)]
    fn get_string(&self, token: &Token) -> &'a str {
        let start = token.position as usize;
        let end = start + token.length as usize;
        std::str::from_utf8(&self.source[start..end]).unwrap_or("")
    }

    #[inline(always)]
    fn push_string(&self, ast: &mut Ast<'a>, s: &'a str) -> u32 {
        let idx = ast.all_strings.len() as u32;
        ast.all_strings.push(s);
        idx
    }

    /// Helper: expect keyword, `{`, parse properties, `}`. Returns (`start_pos`, `end_pos`, `prop_start`, `prop_end`).
    #[inline(always)]
    fn parse_property_block(
        &mut self,
        ast: &mut Ast<'a>,
        keyword: TokenType,
    ) -> Result<(u32, u32, u32, u32), Diagnostic> {
        let start_pos = self.peek(0).position;
        self.expect(keyword)?;
        self.expect(TokenType::LeftBrace)?;
        let (start, end) = self.parse_properties(ast);
        let close = self.expect(TokenType::RightBrace)?;
        let end_pos = self.end_of(close);
        Ok((start_pos, end_pos, start, end))
    }

    /// Helper: parse optional `with ident, ident, ...` mixin list. Returns (`start_idx`, `end_idx`).
    #[inline(always)]
    fn parse_with_mixins(&mut self, ast: &mut Ast<'a>) -> Result<(u32, u32), Diagnostic> {
        let start_idx = ast.all_strings.len() as u32;
        if self.match_token(TokenType::With) {
            let mixin_token = self.expect(TokenType::Identifier)?;
            self.push_string(ast, self.get_string(mixin_token));
            while self.match_token(TokenType::Comma) {
                let t = self.expect(TokenType::Identifier)?;
                self.push_string(ast, self.get_string(t));
            }
        }
        let end_idx = ast.all_strings.len() as u32;
        Ok((start_idx, end_idx))
    }

    #[inline(always)]
    fn parse_statement(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        match self.peek(0).kind {
            TokenType::Project => self.parse_project(ast),
            TokenType::Workspace => self.parse_workspace(ast),
            TokenType::Target => self.parse_target(ast),
            TokenType::Dependencies => self.parse_dependencies(ast),
            TokenType::Options => self.parse_options(ast),
            TokenType::Mixin => self.parse_mixin(ast),
            TokenType::Profile => self.parse_profile(ast),
            TokenType::Install => self.parse_install(ast),
            TokenType::Package => self.parse_package(ast),
            TokenType::Script => self.parse_script(ast),

            TokenType::AtIf => self.parse_if(ast),
            TokenType::AtFor => self.parse_for(ast),
            TokenType::AtBreak | TokenType::AtContinue => self.parse_loop_control(ast),
            TokenType::AtError | TokenType::AtWarning | TokenType::AtInfo | TokenType::AtDebug => {
                self.parse_diagnostic(ast)
            }
            TokenType::Identifier => {
                if self.peek(1).kind == TokenType::Colon {
                    self.parse_property(ast).map(Statement::Property)
                } else {
                    let token = self.advance();
                    Err(Diagnostic::new(
                        format!("unexpected identifier '{}'", self.get_string(token)),
                        token.position,
                        "expected a top-level declaration (project, target, mixin) or a statement (if, for, or property)",
                    ))
                }
            }
            _ => {
                let token = self.advance();
                Err(Diagnostic::new(
                    format!(
                        "unexpected token '{}' - expected a declaration or statement",
                        self.get_string(token)
                    ),
                    token.position,
                    "",
                ))
            }
        }
    }

    #[inline(always)]
    fn parse_statement_block(&mut self, ast: &mut Ast<'a>) -> (u32, u32) {
        let start_idx = ast.all_statements.len() as u32;

        while self.peek(0).kind != TokenType::RightBrace
            && self.peek(0).kind != TokenType::EndOfFile
        {
            let kind = self.peek(0).kind;
            let next_kind = self.peek(1).kind;

            let is_property = (kind == TokenType::Identifier || kind.is_keyword())
                && next_kind == TokenType::Colon;
            let is_visibility = kind == TokenType::Public
                || kind == TokenType::Private
                || kind == TokenType::Interface;

            let stmt_res = if is_property {
                self.parse_property(ast).map(Statement::Property)
            } else if is_visibility {
                self.parse_visibility_block(ast)
            } else {
                self.parse_statement(ast)
            };

            match stmt_res {
                Ok(stmt) => ast.all_statements.push(stmt),
                Err(err) => {
                    ast.errors.push(err);
                    self.synchronize();
                }
            }
        }

        let end_idx = ast.all_statements.len() as u32;
        (start_idx, end_idx)
    }

    #[inline(always)]
    fn parse_properties(&mut self, ast: &mut Ast<'a>) -> (u32, u32) {
        let start_idx = ast.all_properties.len() as u32;
        while self.peek(0).kind != TokenType::RightBrace
            && self.peek(0).kind != TokenType::EndOfFile
        {
            match self.parse_property(ast) {
                Ok(prop) => ast.all_properties.push(prop),
                Err(err) => {
                    ast.errors.push(err);
                    self.synchronize();
                }
            }
        }
        let end_idx = ast.all_properties.len() as u32;
        (start_idx, end_idx)
    }

    #[inline(always)]
    fn parse_property(&mut self, ast: &mut Ast<'a>) -> Result<Property, Diagnostic> {
        let name_token = self.expect_identifier_or_keyword()?;
        let name_idx = self.push_string(ast, self.get_string(name_token));
        self.expect(TokenType::Colon)?;

        let value_start_idx = ast.all_values.len() as u32;
        let val = self.parse_value(ast)?;
        ast.all_values.push(val);

        while self.match_token(TokenType::Comma) {
            let val = self.parse_value(ast)?;
            ast.all_values.push(val);
        }
        let value_end_idx = ast.all_values.len() as u32;

        self.expect(TokenType::Semicolon)?;
        let end_pos = self.prev_end();

        Ok(Property {
            base: NodeBase::new(name_token.position, end_pos),
            name_idx,
            value_start_idx,
            value_end_idx,
        })
    }

    #[inline(always)]
    fn parse_project(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let start_pos = self.peek(0).position;
        self.expect(TokenType::Project)?;
        let name_token = self.expect(TokenType::Identifier)?;
        let name_idx = self.push_string(ast, self.get_string(name_token));

        self.expect(TokenType::LeftBrace)?;
        let (property_start_idx, property_end_idx) = self.parse_properties(ast);
        self.expect(TokenType::RightBrace)?;
        let end_pos = self.prev_end();

        Ok(Statement::ProjectDecl(ProjectDecl {
            base: NodeBase::new(start_pos, end_pos),
            name_idx,
            property_start_idx,
            property_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_workspace(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let (start_pos, end_pos, property_start_idx, property_end_idx) =
            self.parse_property_block(ast, TokenType::Workspace)?;
        Ok(Statement::WorkspaceDecl(WorkspaceDecl {
            base: NodeBase::new(start_pos, end_pos),
            property_start_idx,
            property_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_target(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let start_pos = self.peek(0).position;
        self.expect(TokenType::Target)?;
        let name_token = self.expect(TokenType::Identifier)?;
        let name_idx = self.push_string(ast, self.get_string(name_token));

        let (mixin_start_idx, mixin_end_idx) = self.parse_with_mixins(ast)?;

        self.expect(TokenType::LeftBrace)?;
        let (body_start_idx, body_end_idx) = self.parse_statement_block(ast);
        self.expect(TokenType::RightBrace)?;
        let end_pos = self.prev_end();

        Ok(Statement::TargetDecl(TargetDecl {
            base: NodeBase::new(start_pos, end_pos),
            name_idx,
            mixin_start_idx,
            mixin_end_idx,
            body_start_idx,
            body_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_dependencies(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let start_pos = self.peek(0).position;
        self.expect(TokenType::Dependencies)?;
        self.expect(TokenType::LeftBrace)?;

        let dep_start_idx = ast.all_dependencies.len() as u32;
        while self.peek(0).kind != TokenType::RightBrace
            && self.peek(0).kind != TokenType::EndOfFile
        {
            match self.parse_dependency_spec(ast) {
                Ok(dep) => ast.all_dependencies.push(dep),
                Err(err) => {
                    ast.errors.push(err);
                    self.synchronize();
                }
            }
        }
        let dep_end_idx = ast.all_dependencies.len() as u32;

        self.expect(TokenType::RightBrace)?;
        let end_pos = self.prev_end();

        Ok(Statement::DependenciesDecl(DependenciesDecl {
            base: NodeBase::new(start_pos, end_pos),
            dep_start_idx,
            dep_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_dependency_spec(
        &mut self,
        ast: &mut Ast<'a>,
    ) -> Result<DependencySpec<'a>, Diagnostic> {
        let start_pos = self.peek(0).position;
        let name_token = self.expect(TokenType::Identifier)?;
        let name_idx = self.push_string(ast, self.get_string(name_token));

        let is_optional = self.match_token(TokenType::Question);
        self.expect(TokenType::Colon)?;

        let value = if self.peek(0).kind == TokenType::Identifier
            && self.peek(1).kind == TokenType::LeftParen
        {
            DependencyValue::FunctionCall(self.parse_function_call(ast)?)
        } else if self.peek(0).kind == TokenType::String {
            let str_token = self.expect(TokenType::String)?;
            DependencyValue::String(self.get_string(str_token))
        } else if self.peek(0).kind == TokenType::Identifier {
            let id_token = self.expect(TokenType::Identifier)?;
            let val = self.get_string(id_token);
            if val != "system" {
                return Err(Diagnostic::new(
                    format!("expected version string, function call, or 'system', got '{val}'"),
                    id_token.position,
                    "valid versions are strings like \"1.0.0\", function calls like git() or path(), or the 'system' keyword",
                ));
            }
            let name_idx = self.push_string(ast, "system");
            DependencyValue::FunctionCall(FunctionCall {
                base: NodeBase::new(id_token.position, self.end_of(id_token)),
                name_idx,
                arg_start_idx: ast.all_values.len() as u32,
                arg_end_idx: ast.all_values.len() as u32,
            })
        } else {
            return Err(Diagnostic::new(
                "expected version string, number, or 'system' keyword for dependency value",
                self.peek(0).position,
                "example: package: \"1.2.3\" or package: path(\"../pkg\") or package: system",
            ));
        };

        let mut option_start_idx = ast.all_properties.len() as u32;
        let mut option_end_idx = option_start_idx;
        if self.match_token(TokenType::LeftBrace) {
            let (start, end) = self.parse_properties(ast);
            option_start_idx = start;
            option_end_idx = end;
            self.expect(TokenType::RightBrace)?;
        }

        self.expect(TokenType::Semicolon)?;
        let end_pos = self.prev_end();

        Ok(DependencySpec {
            base: NodeBase::new(start_pos, end_pos),
            name_idx,
            value,
            option_start_idx,
            option_end_idx,
            is_optional,
        })
    }

    #[inline(always)]
    fn parse_function_call(&mut self, ast: &mut Ast<'a>) -> Result<FunctionCall, Diagnostic> {
        let start_pos = self.peek(0).position;
        let name_token = self.expect(TokenType::Identifier)?;
        let name_idx = self.push_string(ast, self.get_string(name_token));

        self.expect(TokenType::LeftParen)?;

        let arg_start_idx = ast.all_values.len() as u32;
        if self.peek(0).kind != TokenType::RightParen {
            let val = self.parse_value(ast)?;
            ast.all_values.push(val);
            while self.match_token(TokenType::Comma) {
                let val = self.parse_value(ast)?;
                ast.all_values.push(val);
            }
        }
        let arg_end_idx = ast.all_values.len() as u32;
        self.expect(TokenType::RightParen)?;
        let end_pos = self.prev_end();

        Ok(FunctionCall {
            base: NodeBase::new(start_pos, end_pos),
            name_idx,
            arg_start_idx,
            arg_end_idx,
        })
    }

    #[inline(always)]
    fn parse_options(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let start_pos = self.peek(0).position;
        self.expect(TokenType::Options)?;
        self.expect(TokenType::LeftBrace)?;

        let option_start_idx = ast.all_options.len() as u32;
        while self.peek(0).kind != TokenType::RightBrace
            && self.peek(0).kind != TokenType::EndOfFile
        {
            match self.parse_option_spec(ast) {
                Ok(spec) => ast.all_options.push(spec),
                Err(err) => {
                    ast.errors.push(err);
                    self.synchronize();
                }
            }
        }
        let option_end_idx = ast.all_options.len() as u32;

        self.expect(TokenType::RightBrace)?;
        let end_pos = self.prev_end();
        Ok(Statement::OptionsDecl(OptionsDecl {
            base: NodeBase::new(start_pos, end_pos),
            option_start_idx,
            option_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_option_spec(&mut self, ast: &mut Ast<'a>) -> Result<OptionSpec<'a>, Diagnostic> {
        let start_pos = self.peek(0).position;
        let name_token = self.expect(TokenType::Identifier)?;
        let name_idx = self.push_string(ast, self.get_string(name_token));

        self.expect(TokenType::Colon)?;
        let default_value = self.parse_value(ast)?;

        let mut constraint_start_idx = ast.all_properties.len() as u32;
        let mut constraint_end_idx = constraint_start_idx;

        if self.match_token(TokenType::LeftBrace) {
            let (start, end) = self.parse_properties(ast);
            constraint_start_idx = start;
            constraint_end_idx = end;
            self.expect(TokenType::RightBrace)?;
        }

        self.expect(TokenType::Semicolon)?;
        let end_pos = self.prev_end();
        Ok(OptionSpec {
            base: NodeBase::new(start_pos, end_pos),
            name_idx,
            default_value,
            constraint_start_idx,
            constraint_end_idx,
        })
    }

    #[inline(always)]
    fn parse_mixin(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let start_pos = self.peek(0).position;
        self.expect(TokenType::Mixin)?;
        let name_token = self.expect(TokenType::Identifier)?;
        let name_idx = self.push_string(ast, self.get_string(name_token));

        self.expect(TokenType::LeftBrace)?;
        let (body_start_idx, body_end_idx) = self.parse_statement_block(ast);
        self.expect(TokenType::RightBrace)?;
        let end_pos = self.prev_end();

        Ok(Statement::MixinDecl(MixinDecl {
            base: NodeBase::new(start_pos, end_pos),
            name_idx,
            body_start_idx,
            body_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_profile(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let start_pos = self.peek(0).position;
        self.expect(TokenType::Profile)?;
        let name_token = self.expect(TokenType::Identifier)?;
        let name_idx = self.push_string(ast, self.get_string(name_token));

        let (mixin_start_idx, mixin_end_idx) = self.parse_with_mixins(ast)?;

        self.expect(TokenType::LeftBrace)?;
        let (property_start_idx, property_end_idx) = self.parse_properties(ast);
        self.expect(TokenType::RightBrace)?;
        let end_pos = self.prev_end();

        Ok(Statement::ProfileDecl(ProfileDecl {
            base: NodeBase::new(start_pos, end_pos),
            name_idx,
            mixin_start_idx,
            mixin_end_idx,
            property_start_idx,
            property_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_install(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let (start_pos, end_pos, property_start_idx, property_end_idx) =
            self.parse_property_block(ast, TokenType::Install)?;
        Ok(Statement::InstallDecl(InstallDecl {
            base: NodeBase::new(start_pos, end_pos),
            property_start_idx,
            property_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_package(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let (start_pos, end_pos, property_start_idx, property_end_idx) =
            self.parse_property_block(ast, TokenType::Package)?;
        Ok(Statement::PackageDecl(PackageDecl {
            base: NodeBase::new(start_pos, end_pos),
            property_start_idx,
            property_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_script(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let (start_pos, end_pos, property_start_idx, property_end_idx) =
            self.parse_property_block(ast, TokenType::Script)?;
        Ok(Statement::ScriptDecl(ScriptDecl {
            base: NodeBase::new(start_pos, end_pos),
            property_start_idx,
            property_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_visibility_block(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let start_pos = self.peek(0).position;
        let visibility = if self.match_token(TokenType::Public) {
            Visibility::Public
        } else if self.match_token(TokenType::Private) {
            Visibility::Private
        } else if self.match_token(TokenType::Interface) {
            Visibility::Interface
        } else {
            let token = self.peek(0);
            return Err(Diagnostic::new(
                "expected visibility level (public, private, or interface)",
                token.position,
                "",
            ));
        };

        self.expect(TokenType::LeftBrace)?;
        let (property_start_idx, property_end_idx) = self.parse_properties(ast);
        self.expect(TokenType::RightBrace)?;
        let end_pos = self.prev_end();

        Ok(Statement::VisibilityBlock(VisibilityBlock {
            base: NodeBase::new(start_pos, end_pos),
            visibility,
            property_start_idx,
            property_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_if(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let start_pos = self.peek(0).position;
        // When called from @else-if branch, AtIf was not emitted — only skip if present
        if self.peek(0).kind == TokenType::AtIf {
            self.advance();
        }
        let condition = self.parse_condition(ast)?;

        self.expect(TokenType::LeftBrace)?;
        let (then_start_idx, then_end_idx) = self.parse_statement_block(ast);
        self.expect(TokenType::RightBrace)?;

        let mut else_start_idx = ast.all_statements.len() as u32;
        let mut else_end_idx = else_start_idx;
        let mut end_pos = self.prev_end();

        if self.peek(0).kind == TokenType::AtElseIf {
            self.advance();
            let else_if = self.parse_if(ast)?;
            else_start_idx = ast.all_statements.len() as u32;
            ast.all_statements.push(else_if);
            else_end_idx = ast.all_statements.len() as u32;
            end_pos = self.prev_end();
        } else if self.peek(0).kind == TokenType::AtElse {
            self.advance();
            self.expect(TokenType::LeftBrace)?;
            let (start, end) = self.parse_statement_block(ast);
            else_start_idx = start;
            else_end_idx = end;
            self.expect(TokenType::RightBrace)?;
            end_pos = self.prev_end();
        }

        Ok(Statement::IfStmt(IfStmt {
            base: NodeBase::new(start_pos, end_pos),
            condition,
            then_start_idx,
            then_end_idx,
            else_start_idx,
            else_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_for(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let start_pos = self.peek(0).position;
        self.expect(TokenType::AtFor)?;
        let var_token = self.expect(TokenType::Identifier)?;
        let variable_name_idx = self.push_string(ast, self.get_string(var_token));

        self.expect(TokenType::In)?;
        let iterable = self.parse_iterable(ast)?;

        self.expect(TokenType::LeftBrace)?;
        let (body_start_idx, body_end_idx) = self.parse_statement_block(ast);
        self.expect(TokenType::RightBrace)?;
        let end_pos = self.prev_end();

        Ok(Statement::ForStmt(ForStmt {
            base: NodeBase::new(start_pos, end_pos),
            variable_name_idx,
            iterable,
            body_start_idx,
            body_end_idx,
        }))
    }

    #[inline(always)]
    fn parse_loop_control(&mut self, _ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let start_pos = self.peek(0).position;
        let control = if self.match_token(TokenType::AtBreak) {
            LoopControl::Break
        } else if self.match_token(TokenType::AtContinue) {
            LoopControl::Continue
        } else {
            let token = self.peek(0);
            return Err(Diagnostic::new(
                format!("expected '@break' or '@continue', got '{}'", self.get_string(token)),
                token.position,
                "loop control statements must be used inside @for loops",
            ));
        };
        self.expect(TokenType::Semicolon)?;
        let end_pos = self.prev_end();
        Ok(Statement::LoopControlStmt(LoopControlStmt {
            base: NodeBase::new(start_pos, end_pos),
            control,
        }))
    }

    #[inline(always)]
    fn parse_diagnostic(&mut self, ast: &mut Ast<'a>) -> Result<Statement, Diagnostic> {
        let start_pos = self.peek(0).position;
        let level = if self.match_token(TokenType::AtError) {
            DiagnosticLevel::Error
        } else if self.match_token(TokenType::AtWarning) {
            DiagnosticLevel::Warning
        } else if self.match_token(TokenType::AtInfo) {
            DiagnosticLevel::Info
        } else if self.match_token(TokenType::AtDebug) {
            DiagnosticLevel::Debug
        } else {
            let token = self.peek(0);
            return Err(Diagnostic::new(
                format!(
                    "expected diagnostic level (@error, @warning, etc), got '{}'",
                    self.get_string(token)
                ),
                token.position,
                "diagnostic statements must start with @error, @warning, @info, or @debug",
            ));
        };

        let message_token = self.expect(TokenType::String)?;
        let message_idx = self.push_string(ast, self.get_string(message_token));
        self.expect(TokenType::Semicolon)?;
        let end_pos = self.prev_end();

        Ok(Statement::DiagnosticStmt(DiagnosticStmt {
            base: NodeBase::new(start_pos, end_pos),
            level,
            message_idx,
        }))
    }

    #[inline(always)]
    fn parse_condition(&mut self, ast: &mut Ast<'a>) -> Result<Condition, Diagnostic> {
        let start_pos = self.peek(0).position;
        let first_comparison = self.parse_comparison_expr(ast)?;

        let kind = self.peek(0).kind;
        if kind == TokenType::And || kind == TokenType::Or {
            let logical_op =
                if kind == TokenType::And { LogicalOperator::And } else { LogicalOperator::Or };

            let operand_start_idx = ast.all_comparison_exprs.len() as u32;
            ast.all_comparison_exprs.push(first_comparison);

            while self.peek(0).kind == kind {
                self.advance();
                let comp = self.parse_comparison_expr(ast)?;
                ast.all_comparison_exprs.push(comp);
            }
            let operand_end_idx = ast.all_comparison_exprs.len() as u32;

            let end_pos = self.prev_end();
            Ok(Condition::LogicalExpr(LogicalExpr {
                base: NodeBase::new(start_pos, end_pos),
                operand_start_idx,
                op: logical_op,
                operand_end_idx,
            }))
        } else if first_comparison.op.is_none() {
            let unary = ast.all_unary_exprs.pop().unwrap();
            Ok(Condition::UnaryExpr(unary))
        } else {
            Ok(Condition::ComparisonExpr(first_comparison))
        }
    }

    #[inline(always)]
    fn parse_comparison_expr(&mut self, ast: &mut Ast<'a>) -> Result<ComparisonExpr, Diagnostic> {
        let start_pos = self.peek(0).position;
        let left = self.parse_unary_expr(ast)?;
        let left_idx = ast.all_unary_exprs.len() as u32;
        ast.all_unary_exprs.push(left);

        let op = match self.peek(0).kind {
            TokenType::Equal => Some(ComparisonOperator::Equal),
            TokenType::NotEqual => Some(ComparisonOperator::NotEqual),
            TokenType::Less => Some(ComparisonOperator::Less),
            TokenType::LessEqual => Some(ComparisonOperator::LessEqual),
            TokenType::Greater => Some(ComparisonOperator::Greater),
            TokenType::GreaterEqual => Some(ComparisonOperator::GreaterEqual),
            _ => None,
        };

        if let Some(operator) = op {
            self.advance();
            let right = self.parse_unary_expr(ast)?;
            let right_idx = ast.all_unary_exprs.len() as u32;
            ast.all_unary_exprs.push(right);

            let end_pos = self.prev_end();
            Ok(ComparisonExpr {
                base: NodeBase::new(start_pos, end_pos),
                left_idx,
                op: Some(operator),
                right_idx: Some(right_idx),
            })
        } else {
            let end_pos = self.prev_end();
            Ok(ComparisonExpr {
                base: NodeBase::new(start_pos, end_pos),
                left_idx,
                op: None,
                right_idx: None,
            })
        }
    }

    #[inline(always)]
    fn parse_unary_expr(&mut self, ast: &mut Ast<'a>) -> Result<UnaryExpr, Diagnostic> {
        let start_pos = self.peek(0).position;
        let is_negated = self.match_token(TokenType::Not);

        if self.match_token(TokenType::LeftParen) {
            let inner = self.parse_condition(ast)?;
            self.expect(TokenType::RightParen)?;
            let end_pos = self.prev_end();

            match inner {
                Condition::LogicalExpr(logical) => {
                    let idx = ast.all_logical_exprs.len() as u32;
                    ast.all_logical_exprs.push(logical);
                    return Ok(UnaryExpr {
                        base: NodeBase::new(start_pos, end_pos),
                        is_negated,
                        operand: UnaryOperand {
                            kind: OperandType::LogicalExpr,
                            idx,
                        },
                    });
                }
                Condition::ComparisonExpr(comparison) => {
                    let operand_start_idx = ast.all_comparison_exprs.len() as u32;
                    ast.all_comparison_exprs.push(comparison);
                    let operand_end_idx = ast.all_comparison_exprs.len() as u32;

                    let logical = LogicalExpr {
                        base: NodeBase::new(start_pos, end_pos),
                        operand_start_idx,
                        op: LogicalOperator::And,
                        operand_end_idx,
                    };
                    let idx = ast.all_logical_exprs.len() as u32;
                    ast.all_logical_exprs.push(logical);

                    return Ok(UnaryExpr {
                        base: NodeBase::new(start_pos, end_pos),
                        is_negated,
                        operand: UnaryOperand {
                            kind: OperandType::LogicalExpr,
                            idx,
                        },
                    });
                }
                Condition::UnaryExpr(inner_unary) => {
                    return Ok(UnaryExpr {
                        base: NodeBase::new(start_pos, end_pos),
                        is_negated: is_negated ^ inner_unary.is_negated,
                        operand: inner_unary.operand,
                    });
                }
            }
        }

        if self.peek(0).kind == TokenType::Identifier && self.peek(1).kind == TokenType::LeftParen {
            let func = self.parse_function_call(ast)?;
            let idx = ast.all_function_calls.len() as u32;
            ast.all_function_calls.push(func);
            let end_pos = self.prev_end();
            return Ok(UnaryExpr {
                base: NodeBase::new(start_pos, end_pos),
                is_negated,
                operand: UnaryOperand {
                    kind: OperandType::FunctionCall,
                    idx,
                },
            });
        }

        let value = self.parse_value(ast)?;
        let idx = ast.all_values.len() as u32;
        ast.all_values.push(value);
        let end_pos = self.prev_end();

        Ok(UnaryExpr {
            base: NodeBase::new(start_pos, end_pos),
            is_negated,
            operand: UnaryOperand {
                kind: OperandType::Value,
                idx,
            },
        })
    }

    #[inline(always)]
    fn parse_iterable(&mut self, ast: &mut Ast<'a>) -> Result<Iterable, Diagnostic> {
        if self.peek(0).kind == TokenType::LeftBracket {
            return self.parse_list(ast).map(Iterable::List);
        }
        if self.peek(0).kind == TokenType::Number && self.peek(1).kind == TokenType::Range {
            return self.parse_range(ast).map(Iterable::Range);
        }
        if self.peek(0).kind == TokenType::Identifier && self.peek(1).kind == TokenType::LeftParen {
            return self.parse_function_call(ast).map(Iterable::FunctionCall);
        }

        let token = self.peek(0);
        Err(Diagnostic::new(
            format!(
                "expected list '[...]', range 'start..end', or function call, got '{}'",
                self.get_string(token)
            ),
            token.position,
            "Examples: [1, 2, 3] or 0..10 or files(\"*.cpp\")",
        ))
    }

    #[inline(always)]
    fn parse_list(&mut self, ast: &mut Ast<'a>) -> Result<List, Diagnostic> {
        let start_pos = self.peek(0).position;
        self.expect(TokenType::LeftBracket)?;

        let element_start_idx = ast.all_values.len() as u32;
        if self.peek(0).kind != TokenType::RightBracket {
            let val = self.parse_value(ast)?;
            ast.all_values.push(val);
            while self.match_token(TokenType::Comma) {
                let val = self.parse_value(ast)?;
                ast.all_values.push(val);
            }
        }
        let element_end_idx = ast.all_values.len() as u32;

        self.expect(TokenType::RightBracket)?;
        let end_pos = self.prev_end();

        Ok(List {
            base: NodeBase::new(start_pos, end_pos),
            element_start_idx,
            element_end_idx,
        })
    }

    #[inline(always)]
    fn parse_range(&mut self, _ast: &mut Ast<'a>) -> Result<Range, Diagnostic> {
        let start_pos = self.peek(0).position;
        let start_token = self.expect(TokenType::Number)?;
        let start_s = self.get_string(start_token);

        self.expect(TokenType::Range)?;
        let end_token = self.expect(TokenType::Number)?;
        let end_s = self.get_string(end_token);

        let start_val: u32 = start_s.parse().map_err(|_| {
            Diagnostic::new(
                format!("invalid range start '{start_s}'"),
                start_token.position,
                "range bounds must be valid unsigned 32-bit numbers",
            )
        })?;
        let end_val: u32 = end_s.parse().map_err(|_| {
            Diagnostic::new(
                format!("invalid range end '{end_s}'"),
                end_token.position,
                "range bounds must be valid unsigned 32-bit numbers",
            )
        })?;

        if start_val > end_val {
            return Err(Diagnostic::new(
                "invalid range values - start must be less than or equal to end",
                start_pos,
                format!("range {start_val}..{end_val} is reversed"),
            ));
        }

        let end_pos = self.end_of(end_token);
        Ok(Range {
            base: NodeBase::new(start_pos, end_pos),
            start_idx: start_val,
            end_idx: end_val,
        })
    }

    #[inline(always)]
    fn parse_value(&mut self, _ast: &mut Ast<'a>) -> Result<Value<'a>, Diagnostic> {
        match self.peek(0).kind {
            TokenType::String => {
                let token = self.advance();
                let s = self.get_string(token);
                Ok(Value::String(s))
            }
            TokenType::Identifier => {
                let token = self.advance();
                Ok(Value::Identifier(self.get_string(token)))
            }
            TokenType::Number => {
                let token = self.advance();
                let s = self.get_string(token);
                let val: u32 = s.parse().map_err(|_| {
                    Diagnostic::new(
                        format!("invalid integer literal '{s}'"),
                        token.position,
                        "integers must be valid unsigned 32-bit numbers",
                    )
                })?;
                Ok(Value::Integer(val))
            }
            TokenType::True => {
                self.advance();
                Ok(Value::Boolean(true))
            }
            TokenType::False => {
                self.advance();
                Ok(Value::Boolean(false))
            }
            _ => {
                let token = self.peek(0);
                Err(Diagnostic::new(
                    format!("expected a value, got '{}'", self.get_string(token)),
                    token.position,
                    "Valid values: \"string\", number, true, false, or identifier",
                ))
            }
        }
    }
}
