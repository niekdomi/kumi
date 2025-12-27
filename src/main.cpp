/// @file main.cpp
/// @brief Entry point for the Kumi build tool

#include "lex/lexer.hpp"
#include "parse/parser.hpp"
#include "support/diagnostic.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

auto main(int argc, char **argv) -> int
{
    if (argc < 2) {
        std::cerr << "Usage: kumi <file.kumi>\n";
        return 1;
    }

    const std::string filename = argv[1];

    // Read file
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: Could not open file '" << filename << "'\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string source = buffer.str();

    // Lex
    kumi::Lexer lexer(source);
    auto tokens_result = lexer.tokenize();

    if (!tokens_result) {
        const auto &error = tokens_result.error();
        kumi::DiagnosticPrinter printer(source);
        printer.print_error(error);
        return 1;
    }

    const auto &tokens = *tokens_result;
    std::cout << "Lexed " << tokens.size() << " tokens\n";

    // Parse
    kumi::Parser parser(tokens);
    auto ast_result = parser.parse();

    if (!ast_result) {
        const auto &error = ast_result.error();
        kumi::DiagnosticPrinter printer(source);
        printer.print_error(error);
        return 1;
    }

    const auto &ast = *ast_result;
    std::cout << "Parsed " << ast.statements.size() << " statements\n";
    std::cout << "Success!\n";

    return 0;
}
