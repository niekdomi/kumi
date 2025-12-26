# Kumi Development Tools

## Building

```bash
# Debug build (default)
xmake

# Release build
xmake f -m release
xmake

# Clean and rebuild
xmake f -c
xmake
```

## Running

```bash
xmake run kumi examples/hello.kumi
```

## Code Quality Tools

### Clang-Format (Auto-formatting)

Format all source files:
```bash
xmake format
```

### Clang-Tidy (Linting)

Run clang-tidy on all files:
```bash
xmake lint
```

Run with warnings as errors:
```bash
xmake lint -e
```

### Both Together (Format + Lint)

```bash
xmake check
```

This runs `clang-format` followed by `clang-tidy` with warnings-as-errors.

## Manual Commands

If you prefer manual control:

```bash
# Format
find src -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Lint
find src -name "*.cpp" -o -name "*.hpp" -exec clang-tidy -warnings-as-errors='*' {} \;
```

## Available Xmake Tasks

- `xmake` - Build (debug by default)
- `xmake run kumi <file>` - Run the kumi executable
- `xmake format` - Format all code with clang-format
- `xmake lint` - Run clang-tidy
- `xmake lint -e` - Run clang-tidy with warnings-as-errors
- `xmake check` - Format + lint with errors
