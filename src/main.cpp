/// @file main.cpp
/// @brief Entry point for the Kumi build tool

#include "lex/lexer.hpp"
// #include "parse/parser.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <string>

auto get_peak_memory_mb() -> double
{
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.starts_with("VmHWM:")) {
            // VmHWM is peak RSS in kB
            std::istringstream iss(line.substr(6));
            long kb = 0;
            iss >> kb;
            return static_cast<double>(kb) / 1024.0; // Convert to MB
        }
    }
    return 0.0;
}

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

    auto start_lex = std::chrono::high_resolution_clock::now();
    auto tokens_result = lexer.tokenize();

    auto end_lex = std::chrono::high_resolution_clock::now();

    auto duration_lex_us
      = std::chrono::duration_cast<std::chrono::microseconds>(end_lex - start_lex);

    // Display file info
    const auto &tokens = *tokens_result;
    double size_mb = static_cast<double>(source.size()) / 1000000.0;

    std::println("╭─────────────────────────────────────────╮");
    std::println("│ File Analysis                           │");
    std::println("├─────────────────────────────────────────┤");
    std::println("│ File:       {:<28}│", filename);
    std::println("│ Size:       {:<7.2f} MB{:<18}│", size_mb, "");
    std::println("│ Tokens:     {:<28}│", tokens.size());
    std::println("╰─────────────────────────────────────────╯");

    // // Parse
    // kumi::Parser parser(tokens);
    // const auto start_parse = std::chrono::high_resolution_clock::now();
    // auto ast_result = parser.parse();
    // const auto end_parse = std::chrono::high_resolution_clock::now();
    // const auto duration_parse_us
    //   = std::chrono::duration_cast<std::chrono::microseconds>(end_parse - start_parse);

    // Display performance metrics
    double duration_lex_ms = static_cast<double>(duration_lex_us.count()) / 1000.0;
    // double duration_parse_ms = static_cast<double>(duration_parse_us.count()) / 1000.0;
    double lex_throughput = size_mb / (duration_lex_ms / 1000.0);
    // double parse_throughput = size_mb / (duration_parse_ms / 1000.0);

    std::println("\n╭─────────────────────────────────────────╮");
    std::println("│ Performance Metrics                     │");
    std::println("├─────────────────────────────────────────┤");
    std::println("│ Lexing:  {:>10.4f} ms {:>10.2f} MB/s  │", duration_lex_ms, lex_throughput);
    std::println("│ Memory:  {:>10.2f} MB (peak RSS)      │", get_peak_memory_mb());
    // std::println("│ Parsing: {:>10.4f} ms {:>10.2f} MB/s  │", duration_parse_ms,
    // parse_throughput); std::println("│ Total:   {:>10.4f} ms {:>10.2f} MB/s  │",
    //  duration_lex_ms + duration_parse_ms,
    //  size_mb / ((duration_lex_ms + duration_parse_ms) / 1000.0));
    std::println("╰─────────────────────────────────────────╯");

    // const auto &ast = *ast_result;
    // std::println("\n✓ Parsed {} statements successfully", ast.statements.size());
    return 0;
}
