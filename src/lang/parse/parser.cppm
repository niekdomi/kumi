module;
#include "lang/support/macros.hpp"

export module kumi.lang.parse;
import std;

import kumi.lang.ast;
import kumi.lang.lex.token;
import kumi.lang.support;

/// @file parser.hpp
/// @brief Recursive descent parser
///
/// @see AST for node definitions
/// @see Lexer for tokenization

export namespace kumi::lang {

/// @brief Recursive descent parser that converts tokens into an AST
///
/// The parser consumes a stream of tokens from the lexer and builds an Abstract
/// Syntax Tree (AST). It uses recursive descent with one-token lookahead.
class Parser final
{
  public:
    /// @brief Constructs a parser for the given source and token stream
    /// @param source Original source text (used to recover token text via position+length)
    /// @param tokens Tokens to parse (must end with END_OF_FILE token)
    explicit Parser(std::string_view source, std::span<const Token> tokens)
        : source_(source),
          tokens_(tokens)
    {}

    /// @brief Parses the token stream into an AST
    /// @return Complete AST on success, Diagnostics on failure
    [[nodiscard]]
    auto parse() -> std::expected<AST, Diagnostics>
    {
        AST ast{};
        ast.statements.reserve(tokens_.size());

        while (peek().type != TokenType::END_OF_FILE) [[likely]] {
            ast.statements.push_back(TRY(parse_statement()));
        }

        return ast;
    }

  private:
    std::string_view source_;       ///< Original source text
    std::span<const Token> tokens_; ///< Token stream being parsed
    std::uint32_t position_{0};     ///< Current position in token stream

    /// @brief Extracts the source text for a token
    [[nodiscard]]
    auto token_text(const Token& token) const noexcept -> std::string_view
    {
        return source_.substr(token.position, token.length);
    }

    /// @brief Advances to next token and returns current token
    auto advance() -> const Token&
    {
        return tokens_[position_++];
    }

    /// @brief Expects a specific token type and consumes it
    [[nodiscard]]
    auto expect(TokenType type) -> std::expected<Token, Diagnostics>
    {
        if (peek().type != type) [[unlikely]] {
            std::string expected_type_str;
            std::uint32_t error_position = peek().position;

            switch (type) {
                case TokenType::LEFT_BRACE:  expected_type_str = "{"; break;
                case TokenType::RIGHT_BRACE: expected_type_str = "}"; break;
                case TokenType::SEMICOLON:
                    expected_type_str = ";";
                    if (position_ > 0) {
                        const auto& prev = tokens_[position_ - 1];
                        error_position = prev.position + prev.length;
                    }
                    break;
                case TokenType::IDENTIFIER: expected_type_str = "IDENTIFIER"; break;
                default:                    expected_type_str = "{, }, ;, IDENTIFIER"; break;
            }

            return error<Token>(
              std::format("expected {}, got {}", expected_type_str, token_text(peek())),
              error_position,
              "");
        }

        return advance();
    }

    /// @brief Matches and consumes a token if it has the expected type
    [[nodiscard]]
    auto match(TokenType type) -> bool
    {
        if (peek().type == type) {
            advance();
            return true;
        }
        return false;
    }

    /// @brief Peeks at next token without advancing
    [[nodiscard]]
    auto peek(std::uint32_t k = 0) const noexcept -> const Token&
    {
        const auto pos = position_ + k;
        if (pos >= tokens_.size()) [[unlikely]] {
            return tokens_.back();
        }
        return tokens_[pos];
    }

    /// @brief Expects an identifier or keyword token (keywords can be used as identifiers)
    [[nodiscard]]
    auto expect_identifier_or_keyword() -> std::expected<Token, Diagnostics>
    {
        const auto is_keyword = [](TokenType t) noexcept -> bool {
            return t >= TokenType::PROJECT && t <= TokenType::FALSE;
        };

        if (peek().type == TokenType::IDENTIFIER || is_keyword(peek().type)) [[likely]] {
            return advance();
        }

        return error<Token>(
          std::format("expected identifier or keyword, got '{}'", token_text(peek())),
          peek().position,
          "expected name here",
          "identifiers must start with a letter or underscore, followed by letters or digits");
    }

    /// @brief Strips surrounding quotes from a string token value
    [[nodiscard]]
    static auto strip_quotes(std::string_view str) noexcept -> std::string
    {
        if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
            str.remove_prefix(1);
            str.remove_suffix(1);
            return std::string{str};
        }
        return std::string{str};
    }

    //===------------------------------------------------------------------===//
    // Parsing Helpers
    //===------------------------------------------------------------------===//

    [[nodiscard]]
    auto parse_dependency_spec(AST& ast) -> std::expected<DependencySpec, Diagnostics>
    {
        const auto start_pos = peek().position;
        const auto name_token = TRY(expect(TokenType::IDENTIFIER));
        const bool is_optional = match(TokenType::QUESTION);
        TRY(expect(TokenType::COLON));

        DependencyValue value;
        if (peek().type == TokenType::IDENTIFIER && peek(1).type == TokenType::LEFT_PAREN) {
            value = TRY(parse_function_call());
        } else if (peek().type == TokenType::STRING) {
            auto str_token = TRY(expect(TokenType::STRING));
            value = strip_quotes(token_text(str_token));
        } else if (peek().type == TokenType::IDENTIFIER) {
            auto id_token = TRY(expect(TokenType::IDENTIFIER));
            if (token_text(id_token) != "system") {
                return error<DependencySpec>(
                  std::format("expected version string, function call, or 'system', got '{}'",
                              token_text(id_token)),
                  id_token.position,
                  "invalid version or specifier",
                  "valid versions are strings like \"1.0.0\", function calls like git() or path(), or the 'system' keyword");
            }
            FunctionCall sys_call{
              .name = "system",
              .arguments = {},
            };
            sys_call.position = id_token.position;
            value = std::move(sys_call);
        } else {
            return error<DependencySpec>(
              "expected version string, number, or 'system' keyword for dependency value",
              peek().position,
              "invalid value",
              "example: package: \"1.2.3\" or package: path(\"../pkg\") or package: system");
        }

        std::vector<Property> options{};
        if (peek().type == TokenType::LEFT_BRACE) {
            advance();
            options = TRY(parse_properties());
            TRY(expect(TokenType::RIGHT_BRACE));
        }

        TRY(expect(TokenType::SEMICOLON));

        DependencySpec spec{
          .is_optional = is_optional,
          .name = std::string{token_text(name_token)},
          .value = std::move(value),
          .options = std::move(options),
        };
        spec.position = start_pos;
        return spec;
    }

    [[nodiscard]]
    auto parse_dependencies(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::DEPENDENCIES));
        TRY(expect(TokenType::LEFT_BRACE));

        std::vector<DependencySpec> dependencies{};
        dependencies.reserve(8);

        while (peek().type != TokenType::RIGHT_BRACE) {
            dependencies.push_back(TRY(parse_dependency_spec()));
        }

        TRY(expect(TokenType::RIGHT_BRACE));

        DependenciesDecl decl{
          .dependencies = std::move(dependencies),
        };
        decl.position = start_pos;
        return decl;
    }

    [[nodiscard]]
    auto parse_diagnostic(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        DiagnosticLevel level{};

        // clang-format off
        if      (match(TokenType::AT_ERROR))   { level = DiagnosticLevel::ERROR;   }
        else if (match(TokenType::AT_WARNING)) { level = DiagnosticLevel::WARNING; }
        else if (match(TokenType::AT_INFO))    { level = DiagnosticLevel::INFO;    }
        else if (match(TokenType::AT_DEBUG))   { level = DiagnosticLevel::DEBUG;   }
        // clang-format on
        else [[unlikely]] {
            return error<Statement>(
              std::format("expected diagnostic level (@error, @warning, etc), got '{}'",
                          token_text(peek())),
              peek().position,
              "unknown directive",
              "diagnostic statements must start with @error, @warning, @info, or @debug");
        }

        const auto message = TRY(expect(TokenType::STRING));
        TRY(expect(TokenType::SEMICOLON));

        return DiagnosticStmt{
          .level = level,
          .message = strip_quotes(token_text(message)),
        };
    }

    //===------------------------------------------------------------------===//
    // Expression Parsing
    //===------------------------------------------------------------------===//

    [[nodiscard]]
    auto parse_condition(AST& ast) -> std::expected<Condition, Diagnostics>
    {
        const auto start_pos = peek().position;
        auto first_comparison = TRY(parse_comparison_expr());

        if (peek().type == TokenType::AND || peek().type == TokenType::OR) {
            const auto op_type = peek().type;
            const auto logical_op =
              (op_type == TokenType::AND) ? LogicalOperator::AND : LogicalOperator::OR;

            std::vector<ComparisonExpr> operands{};
            operands.reserve(4);
            operands.push_back(std::move(first_comparison));

            while (peek().type == op_type) {
                advance();
                operands.push_back(TRY(parse_comparison_expr()));
            }

            LogicalExpr logical{
              .op = logical_op,
              .operands = std::move(operands),
            };
            logical.position = start_pos;
            return Condition{std::move(logical)};
        }

        if (!first_comparison.op.has_value()) {
            return Condition{std::move(first_comparison.left)};
        }

        return Condition{std::move(first_comparison)};
    }

    [[nodiscard]]
    auto parse_comparison_expr(AST& ast) -> std::expected<ComparisonExpr, Diagnostics>
    {
        const auto start_pos = peek().position;
        auto left = TRY(parse_unary_expr());

        std::optional<ComparisonOperator> op{};
        switch (peek().type) {
            case TokenType::EQUAL:         op = ComparisonOperator::EQUAL; break;
            case TokenType::NOT_EQUAL:     op = ComparisonOperator::NOT_EQUAL; break;
            case TokenType::LESS:          op = ComparisonOperator::LESS; break;
            case TokenType::LESS_EQUAL:    op = ComparisonOperator::LESS_EQUAL; break;
            case TokenType::GREATER:       op = ComparisonOperator::GREATER; break;
            case TokenType::GREATER_EQUAL: op = ComparisonOperator::GREATER_EQUAL; break;
            default:                       break;
        }

        if (op.has_value()) {
            advance();
            auto right = TRY(parse_unary_expr());
            ComparisonExpr comp{
              .left = std::move(left),
              .op = op,
              .right = std::move(right),
            };
            comp.position = start_pos;
            return comp;
        }

        ComparisonExpr comp{
          .left = std::move(left),
          .op = std::nullopt,
          .right = std::nullopt,
        };
        comp.position = start_pos;
        return comp;
    }

    [[nodiscard]]
    auto parse_unary_expr(AST& ast) -> std::expected<UnaryExpr, Diagnostics>
    {
        const auto start_pos = peek().position;
        const bool is_negated = match(TokenType::NOT);

        if (peek().type == TokenType::LEFT_PAREN) {
            advance();
            auto inner = TRY(parse_condition());
            TRY(expect(TokenType::RIGHT_PAREN));

            if (auto* logical = std::get_if<LogicalExpr>(&inner)) {
                UnaryExpr unary{
                  .is_negated = is_negated,
                  .operand = std::make_unique<LogicalExpr>(std::move(*logical)),
                };
                unary.position = start_pos;
                return unary;
            }
            if (auto* comparison = std::get_if<ComparisonExpr>(&inner)) {
                std::vector<ComparisonExpr> operands{};
                operands.reserve(1);
                operands.push_back(std::move(*comparison));

                LogicalExpr logical{
                  .op = LogicalOperator::AND,
                  .operands = std::move(operands),
                };
                logical.position = start_pos;
                UnaryExpr unary{
                  .is_negated = is_negated,
                  .operand = std::make_unique<LogicalExpr>(std::move(logical)),
                };
                unary.position = start_pos;
                return unary;
            }
            return error<UnaryExpr>(
              "invalid parenthesized expression",
              peek().position,
              "expected expression",
              "expected a comparison or logical expression inside these parentheses");
        }

        if (peek().type == TokenType::IDENTIFIER && peek(1).type == TokenType::LEFT_PAREN) {
            auto func = TRY(parse_function_call());
            UnaryExpr unary{
              .is_negated = is_negated,
              .operand = std::move(func),
            };
            unary.position = start_pos;
            return unary;
        }

        auto value = TRY(parse_value());
        UnaryExpr unary{
          .is_negated = is_negated,
          .operand = std::move(value),
        };
        unary.position = start_pos;
        return unary;
    }

    [[nodiscard]]
    auto parse_iterable(AST& ast) -> std::expected<Iterable, Diagnostics>
    {
        if (peek().type == TokenType::LEFT_BRACKET) {
            return parse_list();
        }

        if (peek().type == TokenType::NUMBER && peek(1).type == TokenType::RANGE) {
            return parse_range();
        }

        if (peek().type == TokenType::IDENTIFIER && peek(1).type == TokenType::LEFT_PAREN) {
            return parse_function_call();
        }

        return error<Iterable>(
          std::format("expected list '[...]', range 'start..end', or function call, got '{}'",
                      token_text(peek())),
          peek().position,
          "invalid iterable",
          R"(Examples: [1, 2, 3] or 0..10 or files("*.cpp"))");
    }

    [[nodiscard]]
    auto parse_list(AST& ast) -> std::expected<List, Diagnostics>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::LEFT_BRACKET));

        std::vector<Value> elements{};
        elements.reserve(8);

        if (peek().type != TokenType::RIGHT_BRACKET) {
            elements.push_back(TRY(parse_value()));

            while (match(TokenType::COMMA)) {
                elements.push_back(TRY(parse_value()));
            }
        }

        TRY(expect(TokenType::RIGHT_BRACKET));

        List list{
          .elements = std::move(elements),
        };
        list.position = start_pos;
        return list;
    }

    [[nodiscard]]
    auto parse_range(AST& ast) -> std::expected<Range, Diagnostics>
    {
        const auto start_pos = peek().position;
        const auto start_token = TRY(expect(TokenType::NUMBER));
        TRY(expect(TokenType::RANGE));
        const auto end_token = TRY(expect(TokenType::NUMBER));

        std::uint32_t start_val = 0;
        std::uint32_t end_val = 0;

        const auto start_sv = token_text(start_token);
        const auto end_sv = token_text(end_token);

        const auto [_, ec1] =
          std::from_chars(start_sv.data(), start_sv.data() + start_sv.size(), start_val);
        const auto [__, ec2] =
          std::from_chars(end_sv.data(), end_sv.data() + end_sv.size(), end_val);

        if (ec1 != std::errc{} || ec2 != std::errc{}) [[unlikely]] {
            return error<Range>(
              "invalid range values - start must be less than or equal to end",
              start_pos,
              "invalid range",
              std::format(
                "range {}..{} is reversed or contains invalid characters", start_val, end_val));
        }

        Range range{
          .start = start_val,
          .end = end_val,
        };
        range.position = start_pos;
        return range;
    }

    [[nodiscard]]
    auto parse_function_call(AST& ast) -> std::expected<FunctionCall, Diagnostics>
    {
        const auto start_pos = peek().position;
        const auto name_token = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::LEFT_PAREN));

        std::vector<Value> arguments{};
        arguments.reserve(4);

        if (peek().type != TokenType::RIGHT_PAREN) {
            arguments.push_back(TRY(parse_value()));

            while (match(TokenType::COMMA)) {
                arguments.push_back(TRY(parse_value()));
            }
        }

        TRY(expect(TokenType::RIGHT_PAREN));

        FunctionCall func{
          .name = std::string{token_text(name_token)},
          .arguments = std::move(arguments),
        };
        func.position = start_pos;
        return func;
    }

    [[nodiscard]]
    auto parse_for(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::AT_FOR));
        const auto var_token = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::IN));
        auto iterable = TRY(parse_iterable());
        TRY(expect(TokenType::LEFT_BRACE));
        auto body = TRY(parse_statement_block(16));
        TRY(expect(TokenType::RIGHT_BRACE));

        ForStmt stmt{
          .variable = std::string{token_text(var_token)},
          .iterable = std::move(iterable),
          .body = std::move(body),
        };
        stmt.position = start_pos;
        return stmt;
    }

    [[nodiscard]]
    auto parse_if(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::AT_IF));
        auto condition = TRY(parse_condition());
        TRY(expect(TokenType::LEFT_BRACE));
        auto then_block = TRY(parse_statement_block(8));
        TRY(expect(TokenType::RIGHT_BRACE));

        std::vector<Statement> else_block{};
        if (peek().type == TokenType::AT_ELSE_IF) {
            advance();
            else_block.push_back(TRY(parse_if()));
        } else if (peek().type == TokenType::AT_ELSE) {
            advance();
            TRY(expect(TokenType::LEFT_BRACE));
            else_block = TRY(parse_statement_block(8));
            TRY(expect(TokenType::RIGHT_BRACE));
        }

        IfStmt stmt{
          .condition = std::move(condition),
          .then_block = std::move(then_block),
          .else_block = std::move(else_block),
        };
        stmt.position = start_pos;
        return stmt;
    }

    [[nodiscard]]
    auto parse_install(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        TRY(expect(TokenType::INSTALL));
        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return InstallDecl{
          .properties = std::move(properties),
        };
    }

    [[nodiscard]]
    auto parse_loop_control(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        LoopControl control{};

        // clang-format off
        if      (match(TokenType::AT_BREAK))    { control = LoopControl::BREAK;    }
        else if (match(TokenType::AT_CONTINUE)) { control = LoopControl::CONTINUE; }
        // clang-format on
        else [[unlikely]] {
            return error<Statement>(
              std::format("expected '@break' or '@continue', got '{}'", token_text(peek())),
              peek().position,
              "unexpected keyword",
              "loop control statements must be used inside @for loops");
        }

        TRY(expect(TokenType::SEMICOLON));

        return LoopControlStmt{
          .control = control,
        };
    }

    [[nodiscard]]
    auto parse_option_spec(AST& ast) -> std::expected<OptionSpec, Diagnostics>
    {
        const auto start_pos = peek().position;
        const auto name_token = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::COLON));
        auto default_value = TRY(parse_value());

        std::vector<Property> constraints{};
        if (peek().type == TokenType::LEFT_BRACE) {
            advance();
            constraints = TRY(parse_properties());
            TRY(expect(TokenType::RIGHT_BRACE));
        }

        TRY(expect(TokenType::SEMICOLON));

        OptionSpec spec{
          .name = std::string{token_text(name_token)},
          .default_value = std::move(default_value),
          .constraints = std::move(constraints),
        };
        spec.position = start_pos;
        return spec;
    }

    [[nodiscard]]
    auto parse_options(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::OPTIONS));
        TRY(expect(TokenType::LEFT_BRACE));

        std::vector<OptionSpec> options{};
        options.reserve(8);

        while (peek().type != TokenType::RIGHT_BRACE) {
            options.push_back(TRY(parse_option_spec()));
        }

        TRY(expect(TokenType::RIGHT_BRACE));

        OptionsDecl decl{
          .options = std::move(options),
        };
        decl.position = start_pos;
        return decl;
    }

    [[nodiscard]]
    auto parse_mixin(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::MIXIN));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::LEFT_BRACE));
        auto body = TRY(parse_statement_block(16));
        TRY(expect(TokenType::RIGHT_BRACE));

        MixinDecl decl{
          .name = std::string{token_text(identifier)},
          .body = std::move(body),
        };
        decl.position = start_pos;
        return decl;
    }

    [[nodiscard]]
    auto parse_package(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        TRY(expect(TokenType::PACKAGE));
        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return PackageDecl{
          .properties = std::move(properties),
        };
    }

    [[nodiscard]]
    auto parse_project(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        TRY(expect(TokenType::PROJECT));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return ProjectDecl{
          .name = std::string{token_text(identifier)},
          .properties = std::move(properties),
        };
    }

    [[nodiscard]]
    auto parse_properties(AST& ast) -> std::expected<std::vector<Property>, Diagnostics>
    {
        std::vector<Property> properties{};
        properties.reserve(8);

        while (peek().type != TokenType::RIGHT_BRACE) {
            properties.push_back(TRY(parse_property()));
        }

        return properties;
    }

    [[nodiscard]]
    auto parse_property(AST& ast) -> std::expected<Property, Diagnostics>
    {
        const auto identifier = TRY(expect_identifier_or_keyword());
        TRY(expect(TokenType::COLON));

        std::vector<Value> values{};
        values.reserve(4);

        values.push_back(TRY(parse_value()));

        while (match(TokenType::COMMA)) {
            values.push_back(TRY(parse_value()));
        }

        TRY(expect(TokenType::SEMICOLON));

        return Property{
          .key = std::string{token_text(identifier)},
          .values = std::move(values),
        };
    }

    [[nodiscard]]
    auto parse_scripts(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        TRY(expect(TokenType::SCRIPT));
        TRY(expect(TokenType::LEFT_BRACE));
        auto scripts = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return ScriptsDecl{
          .scripts = std::move(scripts),
        };
    }

    [[nodiscard]]
    auto parse_statement(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        switch (peek().type) {
            case TokenType::PROJECT:      return parse_project();
            case TokenType::WORKSPACE:    return parse_workspace();
            case TokenType::TARGET:       return parse_target();
            case TokenType::DEPENDENCIES: return parse_dependencies();
            case TokenType::OPTIONS:      return parse_options();
            case TokenType::MIXIN:        return parse_mixin();
            case TokenType::PROFILE:      return parse_profile();
            case TokenType::INSTALL:      return parse_install();
            case TokenType::PACKAGE:      return parse_package();
            case TokenType::SCRIPT:       return parse_scripts();

            case TokenType::AT_IF:       return parse_if();
            case TokenType::AT_FOR:      return parse_for();
            case TokenType::AT_BREAK:
            case TokenType::AT_CONTINUE: return parse_loop_control();
            case TokenType::AT_ERROR:
            case TokenType::AT_WARNING:
            case TokenType::AT_INFO:
            case TokenType::AT_DEBUG:    return parse_diagnostic();

            case TokenType::IDENTIFIER:
                if (peek(1).type == TokenType::COLON) [[likely]] {
                    return parse_property();
                }
                return error<Statement>(
                  std::format("unexpected identifier '{}'", token_text(peek())),
                  peek().position,
                  "expected declaration or statement",
                  "expected a top-level declaration (project, target, mixin) or a statement (if, for, or property)");

            default:
                [[unlikely]] return error<Statement>(
                  std::format("unexpected token '{}' - expected a declaration or statement",
                              token_text(peek())),
                  peek().position,
                  "invalid token here");
        }
    }

    [[nodiscard]]
    auto parse_statement_block(std::uint32_t reserve_size)
      -> std::expected<std::vector<Statement>, Diagnostics>
    {
        std::vector<Statement> statements{};
        statements.reserve(reserve_size);

        const auto is_keyword = [](TokenType t) noexcept -> bool {
            return t >= TokenType::PROJECT && t <= TokenType::FALSE;
        };

        const auto is_property_token = [&is_keyword](TokenType t,
                                                     TokenType next_t) noexcept -> bool {
            return (t == TokenType::IDENTIFIER || is_keyword(t)) && next_t == TokenType::COLON;
        };

        const auto is_visibility_token = [](TokenType t) noexcept -> bool {
            return t == TokenType::PUBLIC || t == TokenType::PRIVATE || t == TokenType::INTERFACE;
        };

        while (peek().type != TokenType::RIGHT_BRACE) {
            if (is_property_token(peek().type, peek(1).type)) {
                statements.push_back(TRY(parse_property()));
            } else if (is_visibility_token(peek().type)) {
                statements.push_back(TRY(parse_visibility_block()));
            } else {
                statements.push_back(TRY(parse_statement()));
            }
        }

        return statements;
    }

    [[nodiscard]]
    auto parse_profile(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::PROFILE));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));

        std::vector<std::string> mixins{};
        if (match(TokenType::WITH)) {
            mixins.reserve(4);
            mixins.push_back(std::string{token_text(TRY(expect(TokenType::IDENTIFIER)))});

            while (match(TokenType::COMMA)) {
                mixins.push_back(std::string{token_text(TRY(expect(TokenType::IDENTIFIER)))});
            }
        }

        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        ProfileDecl decl{
          .name = std::string{token_text(identifier)},
          .mixins = std::move(mixins),
          .properties = std::move(properties),
        };
        decl.position = start_pos;
        return decl;
    }

    [[nodiscard]]
    auto parse_target(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::TARGET));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));

        std::vector<std::string> mixins{};
        if (match(TokenType::WITH)) {
            mixins.reserve(4);
            mixins.push_back(std::string{token_text(TRY(expect(TokenType::IDENTIFIER)))});

            while (match(TokenType::COMMA)) {
                mixins.push_back(std::string{token_text(TRY(expect(TokenType::IDENTIFIER)))});
            }
        }

        TRY(expect(TokenType::LEFT_BRACE));
        auto body = TRY(parse_statement_block(16));
        TRY(expect(TokenType::RIGHT_BRACE));

        TargetDecl decl{
          .name = std::string{token_text(identifier)},
          .mixins = std::move(mixins),
          .body = std::move(body),
        };
        decl.position = start_pos;
        return decl;
    }

    [[nodiscard]]
    auto parse_value(AST& ast) -> std::expected<Value, Diagnostics>
    {
        switch (peek().type) {
            case TokenType::STRING: {
                auto token = advance();
                auto str = std::string{token_text(token)};
                if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
                    str = str.substr(1, str.size() - 2);
                }
                return Value{std::move(str)};
            }
            case TokenType::IDENTIFIER: {
                auto token = advance();
                return Value{std::string{token_text(token)}};
            }
            case TokenType::NUMBER: {
                const auto token = advance();
                std::uint32_t val = 0;
                const auto sv = token_text(token);
                const auto [_, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);

                if (ec != std::errc{}) [[unlikely]] {
                    return error<Value>(
                      std::format("invalid integer literal '{}'", token_text(token)),
                      token.position,
                      "parse error",
                      "integers must be valid unsigned 32-bit numbers");
                }

                return Value{val};
            }
            case TokenType::TRUE:  advance(); return Value{true};
            case TokenType::FALSE: advance(); return Value{false};
            default:
                [[unlikely]] return error<Value>(
                  std::format("expected a value, got '{}'", token_text(peek())),
                  peek().position,
                  "expected value",
                  R"(Valid values: "string", number, true, false, or identifier)");
        }
    }

    [[nodiscard]]
    auto parse_visibility_block(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        Visibility visibility{};

        // clang-format off
        if      (match(TokenType::PUBLIC))    { visibility = Visibility::PUBLIC;    }
        else if (match(TokenType::PRIVATE))   { visibility = Visibility::PRIVATE;   }
        else if (match(TokenType::INTERFACE)) { visibility = Visibility::INTERFACE; }
        // clang-format on
        else [[unlikely]] {
            return error<Statement>("expected visibility level (public, private, or interface)",
                                    peek().position,
                                    "unknown visibility");
        }

        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return VisibilityBlock{
          .visibility = visibility,
          .properties = std::move(properties),
        };
    }

    [[nodiscard]]
    auto parse_workspace(AST& ast) -> std::expected<Statement, Diagnostics>
    {
        TRY(expect(TokenType::WORKSPACE));
        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return WorkspaceDecl{
          .properties = std::move(properties),
        };
    }
};

} // namespace kumi::lang
