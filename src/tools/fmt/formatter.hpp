#pragma once

#include "lang/ast/ast.hpp"

#include <vector>

// TODO(domi): running `kumi fmt` should invoke the formatter.
// following commands should be possible:
//
// `kumi fmt` -> will format all files recursively in the current directory
// `kumi fmt .`-> will format all files recursively in the current directory
// `kumi fmt dir/ `-> will format all files recursively in the specified directory
// `kumi fmt <file>` -> will format a single file
//
// `kumi fmt --check` -> will check recursively in the current directory check if all files are correctly formatted and exit with non zero if any is not
// `kumi fmt --check .` -> will check recursively in the current directory check if all files are correctly formatted and exit with non zero if any is not
// `kumi fmt --check dir/` -> will check recursively in the specified directory check if all files are correctly formatted and exit with non zero if any is not
// `kumi fmt --check <file>` -> will check a single file if it is correctly formatted and exit with non zero if it is not
//
// Other flags/ideas that could be added are:
//
// - `kumi fmt `--exclude <pattern>` -> exclude files matching the pattern (e.g., `*test.kumi`)
// - `kumi fmt diff` -> show diff of formatting changes instead of rewriting files
// - `kumi fmt verbose` -> show which files were formatted
// - `kumi fmt --config <file>` -> use a specific config file instead of the default kumi-fmt-toml or point to a different path
// - Should we allow passing options via command line? e.g., `kumi fmt --line-length 100 --indent 4`
//
// We should automatically detext a kumi-fmt-toml or .kumi-fmt.toml, which
// configures the formatter options. Following options should be allowed:
//
// line_length = 100
// property_wrap = "auto"  # "auto", "always", "never"
// indent = 4
//
// To keep things simple, we should only wrap property values. If there is a
// heavy nested if else and a property starts at > 0.75 * line_length, we will
// will wrap if there are more than two properties. This is not ideal, but keeps
// things simple and such edge cases realistically never happen in practice.
//
// The formatter should be a token/AST stream similar to gofmt, using Wadler algorithm is hardcore overkill (and could harm performance)
namespace kumi::tools::fmt {

class Formatter
{
  public:
    auto run(const std::vector<lang::AST>& statements) -> void
    {
        // NOTE: I think we should return void, since we should already catch
        // AST / Lexer errors before calling formatter run.
    }

    auto check(const std::vector<lang::AST>& statements, std::string_view input_file) -> int
    {
        // NOTE: Should call `run` and compare the output with the input file.
        // Should exit with 0 if formatting matches, non zero otherwise.
    }

  private:
};

} // namespace kumi::tools::fmt
