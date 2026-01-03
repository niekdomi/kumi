/// @file parser.hpp
/// @brief Recursive descent parser
///
/// @see AST for node definitions
/// @see Lexer for tokenization

#pragma once

#include "ast/ast.hpp"
#include "lex/token.hpp"
#include "support/macros.hpp"
#include "support/parse_error.hpp"

#include <charconv>
#include <expected>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace kumi {

/// @brief Recursive descent parser that converts tokens into an AST
///
/// The parser consumes a stream of tokens from the lexer and builds an Abstract
/// Syntax Tree (AST). It uses recursive descent with one-token lookahead.
class Parser final
{
  public:
    /// @brief Constructs a parser for the given token stream
    /// @param tokens Tokens to parse (must end with END_OF_FILE token)
    explicit Parser(std::span<const Token> tokens) : tokens_(tokens) {}

    /// @brief Parses the token stream into an AST
    /// @return Complete AST on success, ParseError on failure
    [[nodiscard]]
    auto parse() -> std::expected<AST, ParseError>
    {
        AST ast{};
        ast.statements.reserve(tokens_.size());

        while (peek().type != TokenType::END_OF_FILE) [[likely]] {
            ast.statements.push_back(TRY(parse_statement()));
        }

        return ast;
    }

  private:
    std::span<const Token> tokens_; ///< Token stream being parsed
    std::uint32_t position_{0};     ///< Current position in token stream

    /// @brief Advances to next token and returns current token
    /// @return Current token before advancing
    auto advance() -> const Token&
    {
        return tokens_[position_++];
    }

    /// @brief Expects a specific token type and consumes it
    /// @param type Expected token type
    /// @return Token on success, ParseError if type doesn't match
    [[nodiscard]]
    auto expect(TokenType type) -> std::expected<Token, ParseError>
    {
        if (peek().type != type) [[unlikely]] {
            std::string expected_type_str;
            std::uint32_t error_position = peek().position;
            std::string hint_str;

            switch (type) {
                case TokenType::LEFT_BRACE:  expected_type_str = "{"; break;
                case TokenType::RIGHT_BRACE: expected_type_str = "}"; break;
                case TokenType::SEMICOLON:
                    expected_type_str = ";";
                    // For missing semicolon, point to end of previous token
                    // TODO(domi): Use more generic solution so this can also be applied to missing
                    // braces etc.
                    if (position_ > 0) {
                        const auto& prev_token = tokens_[position_ - 1];
                        error_position = prev_token.position + prev_token.value.length();
                    }
                    break;
                case TokenType::IDENTIFIER: expected_type_str = "IDENTIFIER"; break;
                default:                    expected_type_str = "{, }, ;, IDENTIFIER"; break;
            }

            // Create error with hint for semicolons
            auto err =
              error<Token>(std::format("expected {}, got {}", expected_type_str, peek().value),
                           error_position,
                           "");

            // Add hint for missing semicolon
            if (type == TokenType::SEMICOLON && err.has_value() == false) {
                // We can't modify the error directly, so we'll handle hints in main.cpp
            }

            return err;
        }

        return advance();
    }

    /// @brief Matches and consumes a token if it has the expected type
    /// @param type Token type to match
    /// @return true if matched and consumed, false otherwise
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
    /// @param look_ahead Number of tokens to look ahead (default: 0)
    /// @return Token at position + look_ahead, or EOF token if out of bounds
    [[nodiscard]]
    auto peek(std::uint32_t look_ahead = 0) const noexcept -> const Token&
    {
        const auto pos = position_ + look_ahead;
        if (pos >= tokens_.size()) [[unlikely]] {
            return tokens_.back();
        }
        return tokens_[pos];
    }

    /// @brief Expects an identifier or keyword token (keywords can be used as identifiers)
    /// @return Token on success, ParseError if neither identifier nor keyword
    [[nodiscard]]
    auto expect_identifier_or_keyword() -> std::expected<Token, ParseError>
    {
        const auto is_keyword = [](TokenType t) noexcept -> bool {
            return t >= TokenType::PROJECT && t <= TokenType::FALSE;
        };

        if (peek().type == TokenType::IDENTIFIER || is_keyword(peek().type)) [[likely]] {
            return advance();
        }

        return error<Token>(
          std::format("expected identifier or keyword, got '{}'", peek().value),
          peek().position,
          "expected name here",
          "identifiers must start with a letter or underscore, followed by letters or digits");
    }

    /// @brief Strips surrounding quotes from a string token value
    /// @param str The string_view from a STRING token
    /// @return String with quotes removed
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

    /// @brief Parses a single dependency specification
    /// @return DependencySpec
    [[nodiscard]]
    auto parse_dependency_spec() -> std::expected<DependencySpec, ParseError>
    {
        const auto start_pos = peek().position;
        const auto name_token = TRY(expect(TokenType::IDENTIFIER));
        const bool is_optional = match(TokenType::QUESTION);
        TRY(expect(TokenType::COLON));

        // Parse dependency value (version string or function call)
        DependencyValue value;
        if (peek().type == TokenType::IDENTIFIER && peek(1).type == TokenType::LEFT_PAREN) {
            // Function call: git(...), path(...)
            value = TRY(parse_function_call());
        } else if (peek().type == TokenType::STRING) {
            // Version string
            auto str_token = TRY(expect(TokenType::STRING));
            value = strip_quotes(str_token.value);
        } else if (peek().type == TokenType::IDENTIFIER) {
            // system keyword
            auto id_token = TRY(expect(TokenType::IDENTIFIER));
            if (id_token.value != "system") {
                return error<DependencySpec>(
                  std::format("expected version string, function call, or 'system', got '{}'",
                              id_token.value),
                  id_token.position,
                  "invalid version or specifier",
                  "valid versions are strings like \"1.0.0\", function calls like git() or path(), or the 'system' keyword");
            }
            // Represent system as a function call
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

        // Parse optional options block
        std::vector<Property> options{};
        if (peek().type == TokenType::LEFT_BRACE) {
            advance(); // Consume '{'
            options = TRY(parse_properties());
            TRY(expect(TokenType::RIGHT_BRACE));
        }

        TRY(expect(TokenType::SEMICOLON));

        DependencySpec spec{
          .is_optional = is_optional,
          .name = std::string{name_token.value},
          .value = std::move(value),
          .options = std::move(options),
        };
        spec.position = start_pos;
        return spec;
    }

    [[nodiscard]]
    auto parse_dependencies() -> std::expected<Statement, ParseError>
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
    auto parse_diagnostic() -> std::expected<Statement, ParseError>
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
                          peek().value),
              peek().position,
              "unknown directive",
              "diagnostic statements must start with @error, @warning, @info, or @debug");
        }

        const auto message = TRY(expect(TokenType::STRING));
        TRY(expect(TokenType::SEMICOLON));

        return DiagnosticStmt{
          .level = level,
          .message = strip_quotes(message.value),
        };
    }

    //===------------------------------------------------------------------===//
    // Expression Parsing
    //===------------------------------------------------------------------===//

    /// @brief Parses a condition expression for if statements
    /// @return Condition (LogicalExpr | ComparisonExpr | UnaryExpr)
    [[nodiscard]]
    auto parse_condition() -> std::expected<Condition, ParseError>
    {
        const auto start_pos = peek().position;
        auto first_comparison = TRY(parse_comparison_expr());

        // Check for logical operators (and/or)
        if (peek().type == TokenType::AND || peek().type == TokenType::OR) {
            const auto op_type = peek().type;
            const auto logical_op =
              (op_type == TokenType::AND) ? LogicalOperator::AND : LogicalOperator::OR;

            std::vector<ComparisonExpr> operands{};
            operands.reserve(4);
            operands.push_back(std::move(first_comparison));

            while (peek().type == op_type) {
                advance(); // Consume 'and' or 'or'
                operands.push_back(TRY(parse_comparison_expr()));
            }

            LogicalExpr logical{
              .op = logical_op,
              .operands = std::move(operands),
            };
            logical.position = start_pos;
            return Condition{std::move(logical)};
        }

        // Check if it's just a unary expression (no comparison)
        if (!first_comparison.op.has_value()) {
            return Condition{std::move(first_comparison.left)};
        }

        return Condition{std::move(first_comparison)};
    }

    /// @brief Parses a comparison expression
    /// @return ComparisonExpr with optional operator and right side
    [[nodiscard]]
    auto parse_comparison_expr() -> std::expected<ComparisonExpr, ParseError>
    {
        const auto start_pos = peek().position;
        auto left = TRY(parse_unary_expr());

        // Check for comparison operators
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
            advance(); // Consume operator
            auto right = TRY(parse_unary_expr());
            ComparisonExpr comp{
              .left = std::move(left),
              .op = op,
              .right = std::move(right),
            };
            comp.position = start_pos;
            return comp;
        }

        // No comparison operator, just return unary as comparison
        ComparisonExpr comp{
          .left = std::move(left),
          .op = std::nullopt,
          .right = std::nullopt,
        };
        comp.position = start_pos;
        return comp;
    }

    /// @brief Parses a unary expression with optional 'not'
    /// @return UnaryExpr
    [[nodiscard]]
    auto parse_unary_expr() -> std::expected<UnaryExpr, ParseError>
    {
        const auto start_pos = peek().position;
        const bool is_negated = match(TokenType::NOT);

        // Check for parenthesized expression
        if (peek().type == TokenType::LEFT_PAREN) {
            advance(); // Consume '('
            auto inner = TRY(parse_condition());
            TRY(expect(TokenType::RIGHT_PAREN));

            // Wrap in unique_ptr for LogicalExpr variant
            if (auto* logical = std::get_if<LogicalExpr>(&inner)) {
                UnaryExpr unary{
                  .is_negated = is_negated,
                  .operand = std::make_unique<LogicalExpr>(std::move(*logical)),
                };
                unary.position = start_pos;
                return unary;
            }
            if (auto* comparison = std::get_if<ComparisonExpr>(&inner)) {
                // Convert ComparisonExpr to LogicalExpr with single operand
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
            // UnaryExpr case - shouldn't happen with proper grammar
            return error<UnaryExpr>(
              "invalid parenthesized expression",
              peek().position,
              "expected expression",
              "expected a comparison or logical expression inside these parentheses");
        }

        // Check for function call
        if (peek().type == TokenType::IDENTIFIER && peek(1).type == TokenType::LEFT_PAREN) {
            auto func = TRY(parse_function_call());
            UnaryExpr unary{
              .is_negated = is_negated,
              .operand = std::move(func),
            };
            unary.position = start_pos;
            return unary;
        }

        // Parse as value (identifier or boolean)
        auto value = TRY(parse_value());
        UnaryExpr unary{
          .is_negated = is_negated,
          .operand = std::move(value),
        };
        unary.position = start_pos;
        return unary;
    }

    /// @brief Parses an iterable expression for for-loops
    /// @return Iterable (List | Range | FunctionCall)
    [[nodiscard]]
    auto parse_iterable() -> std::expected<Iterable, ParseError>
    {
        // List: [a, b, c]
        if (peek().type == TokenType::LEFT_BRACKET) {
            return parse_list();
        }

        // Range: 0..10
        if (peek().type == TokenType::NUMBER && peek(1).type == TokenType::RANGE) {
            return parse_range();
        }

        // Function call: glob("*.cpp")
        if (peek().type == TokenType::IDENTIFIER && peek(1).type == TokenType::LEFT_PAREN) {
            return parse_function_call();
        }

        return error<Iterable>(
          std::format("expected list '[...]', range 'start..end', or function call, got '{}'",
                      peek().value),
          peek().position,
          "invalid iterable",
          R"(Examples: [1, 2, 3] or 0..10 or files("*.cpp"))");
    }

    /// @brief Parses a list expression
    /// @return List with vector of Values
    [[nodiscard]]
    auto parse_list() -> std::expected<List, ParseError>
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

    /// @brief Parses a range expression
    /// @return Range with start and end values
    [[nodiscard]]
    auto parse_range() -> std::expected<Range, ParseError>
    {
        const auto start_pos = peek().position;
        const auto start_token = TRY(expect(TokenType::NUMBER));
        TRY(expect(TokenType::RANGE));
        const auto end_token = TRY(expect(TokenType::NUMBER));

        // Parse numbers from tokens
        std::uint32_t start_val = 0;
        std::uint32_t end_val = 0;

        const auto [_, ec1] = std::from_chars(
          start_token.value.data(), start_token.value.data() + start_token.value.size(), start_val);
        const auto [__, ec2] = std::from_chars(
          end_token.value.data(), end_token.value.data() + end_token.value.size(), end_val);

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

    /// @brief Parses a function call
    /// @return FunctionCall with name and arguments
    [[nodiscard]]
    auto parse_function_call() -> std::expected<FunctionCall, ParseError>
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
          .name = std::string{name_token.value},
          .arguments = std::move(arguments),
        };
        func.position = start_pos;
        return func;
    }

    [[nodiscard]]
    auto parse_for() -> std::expected<Statement, ParseError>
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
          .variable = std::string{var_token.value},
          .iterable = std::move(iterable),
          .body = std::move(body),
        };
        stmt.position = start_pos;
        return stmt;
    }

    [[nodiscard]]

    auto parse_if() -> std::expected<Statement, ParseError>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::AT_IF));
        auto condition = TRY(parse_condition());
        TRY(expect(TokenType::LEFT_BRACE));
        auto then_block = TRY(parse_statement_block(8));
        TRY(expect(TokenType::RIGHT_BRACE));

        std::vector<Statement> else_block{};
        if (peek().type == TokenType::AT_ELSE_IF) {
            // else-if: consume token and parse as nested if statement
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
    auto parse_import() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::AT_IMPORT));
        const auto path = TRY(expect(TokenType::STRING));
        TRY(expect(TokenType::SEMICOLON));

        return ImportStmt{
          .path = strip_quotes(path.value),
        };
    }

    [[nodiscard]]
    auto parse_install() -> std::expected<Statement, ParseError>
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
    auto parse_loop_control() -> std::expected<Statement, ParseError>
    {
        LoopControl control{};

        // clang-format off
        if      (match(TokenType::AT_BREAK))    { control = LoopControl::BREAK;    }
        else if (match(TokenType::AT_CONTINUE)) { control = LoopControl::CONTINUE; }
        // clang-format on
        else [[unlikely]] {
            return error<Statement>(
              std::format("expected '@break' or '@continue', got '{}'", peek().value),
              peek().position,
              "unexpected keyword",
              "loop control statements must be used inside @for loops");
        }

        TRY(expect(TokenType::SEMICOLON));

        return LoopControlStmt{
          .control = control,
        };
    }

    /// @brief Parses a single option specification
    /// @return OptionSpec
    [[nodiscard]]
    auto parse_option_spec() -> std::expected<OptionSpec, ParseError>
    {
        const auto start_pos = peek().position;
        const auto name_token = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::COLON));
        auto default_value = TRY(parse_value());

        // Parse optional constraints block
        std::vector<Property> constraints{};
        if (peek().type == TokenType::LEFT_BRACE) {
            advance(); // Consume '{'
            constraints = TRY(parse_properties());
            TRY(expect(TokenType::RIGHT_BRACE));
        }

        TRY(expect(TokenType::SEMICOLON));

        OptionSpec spec{
          .name = std::string{name_token.value},
          .default_value = std::move(default_value),
          .constraints = std::move(constraints),
        };
        spec.position = start_pos;
        return spec;
    }

    [[nodiscard]]
    auto parse_options() -> std::expected<Statement, ParseError>
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
    auto parse_mixin() -> std::expected<Statement, ParseError>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::MIXIN));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::LEFT_BRACE));
        auto body = TRY(parse_statement_block(16));
        TRY(expect(TokenType::RIGHT_BRACE));

        MixinDecl decl{
          .name = std::string{identifier.value},
          .body = std::move(body),
        };
        decl.position = start_pos;
        return decl;
    }

    [[nodiscard]]
    auto parse_package() -> std::expected<Statement, ParseError>
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

    auto parse_project() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::PROJECT));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return ProjectDecl{
          .name = std::string{identifier.value},
          .properties = std::move(properties),
        };
    }

    [[nodiscard]]
    auto parse_properties() -> std::expected<std::vector<Property>, ParseError>
    {
        std::vector<Property> properties{};
        properties.reserve(8);

        while (peek().type != TokenType::RIGHT_BRACE) {
            properties.push_back(TRY(parse_property()));
        }

        return properties;
    }

    [[nodiscard]]
    auto parse_property() -> std::expected<Property, ParseError>
    {
        const auto identifier = TRY(expect_identifier_or_keyword());
        TRY(expect(TokenType::COLON));

        std::vector<Value> values{};
        values.reserve(4);

        // Parse comma-separated values
        values.push_back(TRY(parse_value()));

        while (match(TokenType::COMMA)) {
            values.push_back(TRY(parse_value()));
        }

        TRY(expect(TokenType::SEMICOLON));

        return Property{
          .key = std::string{identifier.value},
          .values = std::move(values),
        };
    }

    [[nodiscard]]
    auto parse_scripts() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::SCRIPTS));
        TRY(expect(TokenType::LEFT_BRACE));
        auto scripts = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return ScriptsDecl{
          .scripts = std::move(scripts),
        };
    }

    /// @brief Parses a value or expression for property values
    /// @return Value
    [[nodiscard]]
    auto parse_value_or_expression() -> std::expected<Value, ParseError>
    {
        // Properties can have lists, function calls, or simple values
        // All are now properly represented as Value types
        return parse_value();
    }

    [[nodiscard]]
    auto is_function_start() const noexcept -> bool
    {
        if (peek(1).type != TokenType::LEFT_PAREN) {
            return false;
        }

        // These token types don't exist in the current token set
        // This method appears to be unused legacy code
        return false;
    }

    [[nodiscard]]

    auto parse_statement() -> std::expected<Statement, ParseError>
    {
        switch (peek().type) {
            // Top-level declarations
            case TokenType::PROJECT:      return parse_project();
            case TokenType::WORKSPACE:    return parse_workspace();
            case TokenType::TARGET:       return parse_target();
            case TokenType::DEPENDENCIES: return parse_dependencies();
            case TokenType::OPTIONS:      return parse_options();
            case TokenType::MIXIN:        return parse_mixin();
            case TokenType::PROFILE:      return parse_profile();
            case TokenType::INSTALL:      return parse_install();
            case TokenType::PACKAGE:      return parse_package();
            case TokenType::SCRIPTS:      return parse_scripts();

            // Control flow
            case TokenType::AT_IF:       return parse_if();
            case TokenType::AT_FOR:      return parse_for();
            case TokenType::AT_BREAK:
            case TokenType::AT_CONTINUE: return parse_loop_control();
            case TokenType::AT_ERROR:
            case TokenType::AT_WARNING:
            case TokenType::AT_INFO:
            case TokenType::AT_DEBUG:    return parse_diagnostic();
            case TokenType::AT_IMPORT:   return parse_import();

            // Properties (identifier followed by colon)
            case TokenType::IDENTIFIER:
                if (peek(1).type == TokenType::COLON) [[likely]] {
                    return parse_property();
                }
                return error<Statement>(
                  std::format("unexpected identifier '{}'", peek().value),
                  peek().position,
                  "expected declaration or statement",
                  "expected a top-level declaration (project, target, mixin) or a statement (if, for, or property)");

            default:
                [[unlikely]] return error<Statement>(
                  std::format("unexpected token '{}' - expected a declaration or statement",
                              peek().value),
                  peek().position,
                  "invalid token here");
        }
    }

    [[nodiscard]]
    auto parse_statement_block(std::uint32_t reserve_size)
      -> std::expected<std::vector<Statement>, ParseError>
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

    [[nodiscard]] [[nodiscard]]
    auto parse_profile() -> std::expected<Statement, ParseError>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::PROFILE));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));

        // Parse optional "with" keyword and mixin list
        std::vector<std::string> mixins{};
        if (match(TokenType::WITH)) {
            mixins.reserve(4);
            mixins.push_back(std::string{TRY(expect(TokenType::IDENTIFIER)).value});

            while (match(TokenType::COMMA)) {
                mixins.push_back(std::string{TRY(expect(TokenType::IDENTIFIER)).value});
            }
        }

        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        ProfileDecl decl{
          .name = std::string{identifier.value},
          .mixins = std::move(mixins),
          .properties = std::move(properties),
        };
        decl.position = start_pos;
        return decl;
    }

    [[nodiscard]]
    auto parse_target() -> std::expected<Statement, ParseError>
    {
        const auto start_pos = peek().position;
        TRY(expect(TokenType::TARGET));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));

        // Parse optional "with" keyword and mixin list
        std::vector<std::string> mixins{};
        if (match(TokenType::WITH)) {
            mixins.reserve(4);
            mixins.push_back(std::string{TRY(expect(TokenType::IDENTIFIER)).value});

            while (match(TokenType::COMMA)) {
                mixins.push_back(std::string{TRY(expect(TokenType::IDENTIFIER)).value});
            }
        }

        TRY(expect(TokenType::LEFT_BRACE));
        auto body = TRY(parse_statement_block(16));
        TRY(expect(TokenType::RIGHT_BRACE));

        TargetDecl decl{
          .name = std::string{identifier.value},
          .mixins = std::move(mixins),
          .body = std::move(body),
        };
        decl.position = start_pos;
        return decl;
    }

    [[nodiscard]]

    auto parse_value() -> std::expected<Value, ParseError>
    {
        switch (peek().type) {
            case TokenType::STRING: {
                auto token = advance();
                // Strip surrounding quotes from string literals
                auto str = std::string{token.value};
                if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
                    str = str.substr(1, str.size() - 2);
                }
                return Value{std::move(str)};
            }
            case TokenType::IDENTIFIER: {
                auto token = advance();
                return Value{std::string{token.value}};
            }
            case TokenType::NUMBER: {
                const auto token = advance();
                std::uint32_t val = 0;

                const auto [_, ec] =
                  std::from_chars(token.value.data(), token.value.data() + token.value.size(), val);

                if (ec != std::errc{}) [[unlikely]] {
                    return error<Value>(std::format("invalid integer literal '{}'", token.value),
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
                  std::format("expected a value, got '{}'", peek().value),
                  peek().position,
                  "expected value",
                  R"(Valid values: "string", number, true, false, or identifier)");
        }
    }

    [[nodiscard]]
    auto parse_visibility_block() -> std::expected<Statement, ParseError>
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
    auto parse_workspace() -> std::expected<Statement, ParseError>
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

} // namespace kumi
