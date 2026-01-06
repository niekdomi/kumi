This is a placeholder file. Its planned to integrate external tools such as:

- clang-tidy
- clang-format
- cppcheck

so they can be easily be executed via the cli e.g.,

- `kumi tool lint` -> Runs `run-clang-tidy` (multithreaded)
- `kumi tool fmt` -> Runs `clang-format`
- `kumi tool check` -> Runs `cppcheck`

Things like clang-tidy could be configured to be as fast as possible (as
mentioned run-clang-tidy) so there is less configuration and better ux for the
developer.
