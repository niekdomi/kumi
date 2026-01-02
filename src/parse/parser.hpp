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

#include <cstddef>
#include <expected>
#include <format>
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
    std::size_t position_{0};       ///< Current position in token stream

    /// @brief Advances to next token and returns current token
    /// @return Current token before advancing
    auto advance() -> const Token &
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
            std::size_t error_position = peek().position;
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
                        const auto &prev_token = tokens_[position_ - 1];
                        error_position = prev_token.position + prev_token.value.length();
                    }
                    break;
                case TokenType::IDENTIFIER: expected_type_str = "IDENTIFIER"; break;
                default:                    expected_type_str = "{, }, ;, IDENTIFIER"; break;
            }

            // Create error with hint for semicolons
            auto err = error<Token>(
              std::format("expected {}, got {}", expected_type_str, peek().value), error_position);

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
    auto peek(std::size_t look_ahead = 0) const noexcept -> const Token &
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
            return t >= TokenType::PROJECT && t <= TokenType::SYSTEM;
        };

        if (peek().type == TokenType::IDENTIFIER || is_keyword(peek().type)) [[likely]] {
            return advance();
        }

        return error<Token>(std::format("expected identifier or keyword, got '{}'", peek().value),
                            peek().position);
    }

    //===---------------------------------------------------------------------===//
    // Parsing Helpers
    //===---------------------------------------------------------------------===//

    [[nodiscard]]
    auto parse_apply() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::APPLY_KEYWORD));

        // Check if next token looks like it could be a function name without parentheses
        if (peek().type == TokenType::IDENTIFIER && peek(1).type != TokenType::LEFT_PAREN) {
            return error<Statement>(
              std::format("@apply requires function call syntax: '{}()'", peek().value),
              peek().position);
        }

        auto profile_expr = TRY(parse_function_call());

        // Extract FunctionCall from Expression variant
        if (const auto *func_call = std::get_if<FunctionCall>(&profile_expr)) [[likely]]
        {
            std::vector<Statement> body{};

            if (peek().type == TokenType::LEFT_BRACE) {
                TRY(expect(TokenType::LEFT_BRACE));
                body = TRY(parse_statement_block(8));
                TRY(expect(TokenType::RIGHT_BRACE));
            } else {
                TRY(expect(TokenType::SEMICOLON));
            }

            return ApplyStmt{
              .profile = *func_call,
              .body = std::move(body),
            };
        }

        return error<Statement>(
          std::format("expected function call after @apply, got '{}'", peek().value),
          peek().position);
    }

    [[nodiscard]]
    auto parse_dependencies() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::DEPENDENCIES));
        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return DependenciesDecl{
          .dependencies = std::move(properties),
        };
    }

    [[nodiscard]]
    auto parse_diagnostic() -> std::expected<Statement, ParseError>
    {
        DiagnosticLevel level{};

        // clang-format off
        // NOLINTBEGIN
        if      (match(TokenType::ERROR))    level = DiagnosticLevel::ERROR;
        else if (match(TokenType::WARNING)) level = DiagnosticLevel::WARNING;
        else if (match(TokenType::INFO))    level = DiagnosticLevel::INFO;
        //NOLINTEND
        // clang-format on
        else [[unlikely]] {
            return error<Statement>(
              std::format("expected @error, @warning, or @info, got '{}'", peek().value),
              peek().position);
        }

        const auto message = TRY(expect(TokenType::STRING));
        TRY(expect(TokenType::SEMICOLON));

        return DiagnosticStmt{
          .level = level,
          .message = message.value,
        };
    }

    [[nodiscard]]
    auto parse_expression() -> std::expected<Expression, ParseError>
    {
        const auto is_keyword = [](TokenType t) noexcept -> bool {
            return t >= TokenType::PROJECT && t <= TokenType::SYSTEM;
        };

        const auto is_function_call =
          (peek().type == TokenType::IDENTIFIER || is_keyword(peek().type))
          && peek(1).type
          == TokenType::LEFT_PAREN;

        if (is_function_call) {
            return parse_function_call();
        }

        // Handle list syntax: [value1, value2, ...]
        if (peek().type == TokenType::LEFT_BRACKET) {
            // Parse list as a value (will be serialized as string)
            auto list_value = TRY(parse_value_or_expression());
            return Expression{list_value};
        }

        return parse_value();
    }

    [[nodiscard]]
    auto parse_function_call() -> std::expected<Expression, ParseError>
    {
        const auto name_token = TRY(expect_identifier_or_keyword());
        TRY(expect(TokenType::LEFT_PAREN));

        std::vector<Value> arguments{};

        if (peek().type != TokenType::RIGHT_PAREN) {
            arguments.push_back(TRY(parse_value()));

            while (match(TokenType::COMMA)) {
                arguments.push_back(TRY(parse_value()));
            }
        }

        TRY(expect(TokenType::RIGHT_PAREN));

        return FunctionCall{
          .name = name_token.value,
          .arguments = std::move(arguments),
        };
    }

    [[nodiscard]]
    auto parse_for() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::FOR));
        const auto var_token = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::IN));
        auto iterable = TRY(parse_expression());
        TRY(expect(TokenType::LEFT_BRACE));
        auto body = TRY(parse_statement_block(16));
        TRY(expect(TokenType::RIGHT_BRACE));

        return ForStmt{
          .variable = var_token.value,
          .iterable = std::move(iterable),
          .body = std::move(body),
        };
    }

    [[nodiscard]]
    auto parse_global() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::GLOBAL));
        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return GlobalDecl{
          .properties = std::move(properties),
        };
    }

    [[nodiscard]]
    auto parse_if() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::IF));
        auto condition = TRY(parse_expression());
        TRY(expect(TokenType::LEFT_BRACE));
        auto then_block = TRY(parse_statement_block(8));
        TRY(expect(TokenType::RIGHT_BRACE));

        std::vector<Statement> else_block{};
        if (peek().type == TokenType::ELSE) {
            advance();

            if (peek().type == TokenType::IF) {
                // else-if
                else_block.push_back(TRY(parse_if()));
            } else {
                TRY(expect(TokenType::LEFT_BRACE));
                else_block = TRY(parse_statement_block(8));
                TRY(expect(TokenType::RIGHT_BRACE));
            }
        }

        return IfStmt{
          .condition = std::move(condition),
          .then_block = std::move(then_block),
          .else_block = std::move(else_block),
        };
    }

    [[nodiscard]]
    auto parse_import() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::IMPORT_KEYWORD));
        const auto path = TRY(expect(TokenType::STRING));
        TRY(expect(TokenType::SEMICOLON));

        return ImportStmt{
          .path = path.value,
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
        // NOLINTBEGIN
        if      (match(TokenType::BREAK))    control = LoopControl::BREAK;
        else if (match(TokenType::CONTINUE)) control = LoopControl::CONTINUE;
        // NOLINTEND
        // clang-format on
        else [[unlikely]] {
            return error<Statement>(
              std::format("expected @break or @continue, got '{}'", peek().value), peek().position);
        }

        TRY(expect(TokenType::SEMICOLON));

        return LoopControlStmt{
          .control = control,
        };
    }

    [[nodiscard]]
    auto parse_mixin() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::MIXIN));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::LEFT_BRACE));

        std::vector<Property> properties{};
        std::vector<VisibilityBlock> visibility_blocks{};

        const auto is_visibility = [](TokenType type) -> bool {
            // clang-format off
            return type == TokenType::PUBLIC  || type
                        == TokenType::PRIVATE || type
                        == TokenType::INTERFACE;
            // clang-format on
        };

        while (peek().type != TokenType::RIGHT_BRACE) {
            if (is_visibility(peek().type)) {
                auto stmt = TRY(parse_visibility_block());
                if (auto *vis = std::get_if<VisibilityBlock>(&stmt)) {
                    visibility_blocks.push_back(std::move(*vis));
                }
            } else {
                auto stmt = TRY(parse_property());
                if (auto *prop = std::get_if<Property>(&stmt)) {
                    properties.push_back(std::move(*prop));
                }
            }
        }

        TRY(expect(TokenType::RIGHT_BRACE));

        return MixinDecl{
          .name = identifier.value,
          .properties = std::move(properties),
          .visibility_blocks = std::move(visibility_blocks),
        };
    }

    [[nodiscard]]
    auto parse_options() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::OPTIONS));
        TRY(expect(TokenType::LEFT_BRACE));
        auto options = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return OptionsDecl{
          .options = std::move(options),
        };
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
    auto parse_preset() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::PRESET));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return PresetDecl{
          .name = identifier.value,
          .properties = std::move(properties),
        };
    }

    [[nodiscard]]
    auto parse_features() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::FEATURES));
        TRY(expect(TokenType::LEFT_BRACE));
        auto features = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return FeaturesDecl{
          .features = std::move(features),
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
          .name = identifier.value,
          .properties = std::move(properties),
        };
    }

    [[nodiscard]]
    auto parse_properties() -> std::expected<std::vector<Property>, ParseError>
    {
        std::vector<Property> properties{};
        properties.reserve(8);

        while (peek().type != TokenType::RIGHT_BRACE) {
            auto statement = TRY(parse_property());

            if (const auto *property = std::get_if<Property>(&statement)) [[likely]]
            {
                properties.push_back(*property);
                continue;
            }

            return error<std::vector<Property>>(
              std::format("expected property (key: value;), got '{}'", peek().value),
              peek().position);
        }

        return properties;
    }

    [[nodiscard]]
    auto parse_property() -> std::expected<Statement, ParseError>
    {
        const auto identifier = TRY(expect_identifier_or_keyword());
        TRY(expect(TokenType::COLON));

        std::vector<Value> values{};
        values.reserve(4);

        // Parse comma-separated values
        values.push_back(TRY(parse_value_or_expression()));

        while (match(TokenType::COMMA)) {
            values.push_back(TRY(parse_value_or_expression()));
        }

        TRY(expect(TokenType::SEMICOLON));

        return Property{
          .key = identifier.value,
          .values = std::move(values),
        };
    }

    [[nodiscard]]
    auto parse_value_or_expression() -> std::expected<Value, ParseError>
    {
        // List syntax: [value1, value2, value3]
        if (peek().type == TokenType::LEFT_BRACKET) {
            advance(); // consume '['

            // Empty list
            if (peek().type == TokenType::RIGHT_BRACKET) {
                advance(); // consume ']'
                return Value{std::string{"[]"}};
            }

            // Parse list items
            std::string list_str = "[";
            bool first = true;

            while (peek().type != TokenType::RIGHT_BRACKET) {
                if (!first) {
                    if (!match(TokenType::COMMA)) {
                        return error<Value>(
                          std::format("expected ',' or ']' in list, got '{}'", peek().value),
                          peek().position);
                    }
                    list_str += ", ";
                }
                first = false;

                // Parse list item value
                auto item = TRY(parse_value());
                if (auto *str = std::get_if<std::string>(&item)) {
                    list_str += *str;
                } else if (auto *num = std::get_if<int>(&item)) {
                    list_str += std::to_string(*num);
                } else if (auto *b = std::get_if<bool>(&item)) {
                    list_str += *b ? "true" : "false";
                }
            }

            TRY(expect(TokenType::RIGHT_BRACKET));
            list_str += "]";
            return Value{list_str};
        }

        // Function call (var, glob, etc.)
        if (is_function_start()) {
            auto expr = TRY(parse_expression());

            // Convert Expression to Value by serializing function calls
            // Since Value is std::variant<string, int, bool>, we store function calls as strings
            if (auto *fc = std::get_if<FunctionCall>(&expr)) {
                // Serialize function call: "var(--std)" becomes "var(--std)"
                std::string serialized = fc->name + "(";
                for (size_t i = 0; i < fc->arguments.size(); ++i) {
                    if (i > 0) {
                        serialized += ", ";
                    }
                    // Serialize each argument
                    if (auto *str = std::get_if<std::string>(&fc->arguments[i])) {
                        serialized += *str;
                    } else if (auto *num = std::get_if<int>(&fc->arguments[i])) {
                        serialized += std::to_string(*num);
                    } else if (auto *b = std::get_if<bool>(&fc->arguments[i])) {
                        serialized += *b ? "true" : "false";
                    }
                }
                serialized += ")";
                return Value{serialized};
            }

            // If it's a Variable expression, serialize it
            if (auto *var = std::get_if<Variable>(&expr)) {
                return Value{var->name};
            }

            // For other expression types, return a placeholder
            return Value{std::string{"<expression>"}};
        }

        return parse_value();
    }

    [[nodiscard]]
    auto is_function_start() const noexcept -> bool
    {
        if (peek(1).type != TokenType::LEFT_PAREN) {
            return false;
        }

        switch (peek().type) {
            case TokenType::VAR:
            case TokenType::GLOB:
            case TokenType::GIT:
            case TokenType::URL:
            case TokenType::PLATFORM:
            case TokenType::ARCH:
            case TokenType::COMPILER:
            case TokenType::CONFIG:
            case TokenType::OPTION:
            case TokenType::FEATURE:
            case TokenType::HAS_FEATURE:
            case TokenType::EXISTS:
            case TokenType::SYSTEM:      return true;
            default:                     return false;
        }
    }

    [[nodiscard]]
    auto parse_root() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::ROOT));
        TRY(expect(TokenType::LEFT_BRACE));

        // Parse variable declarations (--name: value;)
        std::vector<Property> variables{};
        while (peek().type != TokenType::RIGHT_BRACE) {
            // Expect variable token (--name)
            if (peek().type != TokenType::VARIABLE) {
                return error<Statement>(
                  std::format("expected variable (--name), got {}", peek().value), peek().position);
            }

            auto var_token = advance(); // Consume the VARIABLE token (e.g., "--std")

            TRY(expect(TokenType::COLON));
            auto value = TRY(parse_value());
            TRY(expect(TokenType::SEMICOLON));

            variables.push_back(Property{
              .key = var_token.value, // Already includes "--" prefix
              .values = {std::move(value)},
            });
        }

        TRY(expect(TokenType::RIGHT_BRACE));

        return RootDecl{
          .variables = std::move(variables),
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
            case TokenType::GLOBAL:       return parse_global();
            case TokenType::MIXIN:        return parse_mixin();
            case TokenType::PRESET:       return parse_preset();
            case TokenType::FEATURES:     return parse_features();
            case TokenType::TESTING:      return parse_testing();
            case TokenType::INSTALL:      return parse_install();
            case TokenType::PACKAGE:      return parse_package();
            case TokenType::SCRIPTS:      return parse_scripts();
            case TokenType::TOOLCHAIN:    return parse_toolchain();
            case TokenType::ROOT:         return parse_root();

            // :root declaration (colon + root)
            case TokenType::COLON:
                if (peek(1).type == TokenType::ROOT) {
                    advance(); // consume ':'
                    return parse_root();
                }
                return error<Statement>(
                  std::format("unexpected token ':' - expected a declaration or statement"),
                  peek().position);

            // Control flow
            case TokenType::IF:             return parse_if();
            case TokenType::FOR:            return parse_for();
            case TokenType::BREAK:
            case TokenType::CONTINUE:       return parse_loop_control();
            case TokenType::ERROR:
            case TokenType::WARNING:
            case TokenType::INFO:           return parse_diagnostic();
            case TokenType::IMPORT_KEYWORD: return parse_import();
            case TokenType::APPLY_KEYWORD:  return parse_apply();

            // Properties (identifier followed by colon)
            case TokenType::IDENTIFIER:
                if (peek(1).type == TokenType::COLON) [[likely]] {
                    return parse_property();
                }
                return error<Statement>(
                  std::format("unexpected identifier '{}' - expected a declaration or statement",
                              peek().value),
                  peek().position);

            default:
                [[unlikely]] return error<Statement>(
                  std::format("unexpected token '{}' - expected a declaration or statement",
                              peek().value),
                  peek().position);
        }
    }

    [[nodiscard]]
    auto parse_statement_block(std::size_t reserve_size)
      -> std::expected<std::vector<Statement>, ParseError>
    {
        std::vector<Statement> statements{};
        statements.reserve(reserve_size);

        const auto is_keyword = [](TokenType t) noexcept -> bool {
            return t >= TokenType::PROJECT && t <= TokenType::SYSTEM;
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
    auto parse_target() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::TARGET));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));

        // Check for optional "extends" keyword
        std::optional<std::string> extends_from;
        if (match(TokenType::EXTENDS)) {
            auto base_name = TRY(expect(TokenType::IDENTIFIER));
            extends_from = base_name.value;
        }

        TRY(expect(TokenType::LEFT_BRACE));
        auto body = TRY(parse_statement_block(16));
        TRY(expect(TokenType::RIGHT_BRACE));

        return TargetDecl{
          .name = identifier.value,
          .extends_from = extends_from,
          .body = std::move(body),
        };
    }

    [[nodiscard]]
    auto parse_testing() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::TESTING));
        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return TestingDecl{
          .properties = std::move(properties),
        };
    }

    [[nodiscard]]
    auto parse_toolchain() -> std::expected<Statement, ParseError>
    {
        TRY(expect(TokenType::TOOLCHAIN));
        const auto identifier = TRY(expect(TokenType::IDENTIFIER));
        TRY(expect(TokenType::LEFT_BRACE));
        auto properties = TRY(parse_properties());
        TRY(expect(TokenType::RIGHT_BRACE));

        return ToolchainDecl{
          .name = identifier.value,
          .properties = std::move(properties),
        };
    }

    [[nodiscard]]
    auto parse_value() -> std::expected<Value, ParseError>
    {
        switch (peek().type) {
            case TokenType::STRING:
            case TokenType::IDENTIFIER:
            case TokenType::VARIABLE:   {
                auto token = advance();
                return Value{std::move(token.value)};
            }
            case TokenType::NUMBER: {
                const auto token = advance();
                int val = 0;

                const auto [_, ec] =
                  std::from_chars(token.value.data(), token.value.data() + token.value.size(), val);

                if (ec != std::errc{}) [[unlikely]] {
                    return error<Value>("invalid integer literal", token.position);
                }

                return Value{val};
            }
            case TokenType::TRUE:  advance(); return Value{true};
            case TokenType::FALSE: advance(); return Value{false};
            default:
                [[unlikely]] return error<Value>(
                  std::format("expected a value, got '{}'", peek().value), peek().position);
        }
    }

    [[nodiscard]]
    auto parse_visibility_block() -> std::expected<Statement, ParseError>
    {
        Visibility visibility{};

        if (match(TokenType::PUBLIC)) {
            visibility = Visibility::PUBLIC;
        } else if (match(TokenType::PRIVATE)) {
            visibility = Visibility::PRIVATE;
        } else if (match(TokenType::INTERFACE)) {
            visibility = Visibility::INTERFACE;
        } else [[unlikely]] {
            return error<Statement>("expected visibility level (public, private, or interface)",
                                    peek().position);
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
