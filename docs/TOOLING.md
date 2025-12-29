# Kumi Tooling: LSP & Formatter

This document outlines the architecture and design for Kumi's developer tooling: the Language Server Protocol (LSP) implementation and code formatter.

---

## Architecture Overview

### Shared Foundation

Both LSP and formatter share the same core components:

```
Source Code
    ↓
Lexer (200 MB/s)
    ↓
Parser (80 MB/s)
    ↓
AST
    ↓
┌───────────┬────────────┐
↓           ↓            ↓
Builder     LSP      Formatter
```

**Key Benefits:**
- Single codebase for lexing/parsing
- Always in sync (no version mismatches)
- Fast performance (< 1ms for typical files)
- Shared semantic analysis

---

## LSP (Language Server Protocol)

### Deployment

**Built into main binary** via subcommand:

```bash
kumi lsp          # Start LSP server (runs a server)
kumi fmt          # Format mode
```

**Why built-in:**
- Single binary distribution
- No version sync issues
- Smaller total footprint
- Simpler for users

### Features

**Core LSP Features:**
- ✅ **Diagnostics** - Real-time error checking
- ✅ **Autocomplete** - Context-aware completions
- ✅ **Go to Definition** - Navigate to mixin/dependency declarations
- ✅ **Hover Documentation** - Show doc comments (`///`)
- ✅ **Syntax Highlighting** - Semantic token provider
- ✅ **Outline View** - Document symbols

**Kumi-Specific Features:**
- Property validation (context-aware)
- String interpolation autocomplete (`${project.█}`)
- Mixin composition validation
- Dependency version checking
- Cross-file navigation (workspace support)

### Update Behavior

**Update on change, not save** (like clangd):
- Parse on every keystroke
- Show errors immediately
- No need to save file
- Fast enough (< 1ms parse time)

### Workspace Resolution

Uses `workspace { }` block to discover projects:

```css
// Root: my-project/kumi.css
workspace {
  members: "core", "renderer", "tools/editor";
}
```

LSP loads all member projects for cross-project features.

### Doc Comments

**Syntax:** `///` for documentation comments

```css
/// Strict warning configuration for production builds.
/// Enables all warnings and treats them as errors.
mixin strict-warnings {
  warnings: all;
  warnings-as-errors: true;
}
```

**Implementation:**
- Lexer emits all comments as `COMMENT` tokens
- LSP detects `///` prefix for doc comments

### Configuration

**No config file needed** - LSP uses sensible defaults.

Optional: `.editorconfig` for editor-wide settings.

---

## Formatter

### Algorithm

**Recursive AST Walker:**

### Philosophy

**Opinionated with minimal configuration**:
- One obvious way to format Kumi files
- Focus on writing code, not formatting
- `.editorconfig` will superset line-width and indent settings

### Configuration

**File:** `.kumi-format`

**Settings:**

```toml
# Line width (default: 80)
line-width = 80

# Indentation (default: 2, ALWAYS spaces)
indent = 2

# List wrapping behavior (default: "auto")
# - "auto": wrap if exceeds line-width
# - "always": always wrap lists
# - "never": keep on one line if possible
wrap-lists = "auto"
```

### Examples

**Before formatting:**
```css
target myapp{type:executable;sources:"a.cpp","b.cpp","c.cpp";depends:fmt,spdlog,imgui;}
```

**After formatting (wrap-lists = "auto", line-width = 100):**
```css
target myapp {
  type: executable;
  sources: "a.cpp", "b.cpp", "c.cpp";
  depends: fmt, spdlog, imgui;
}
```

**After formatting (wrap-lists = "always"):**
```css
target myapp {
  type: executable;
  sources:
    "a.cpp",
    "b.cpp",
    "c.cpp";
  depends:
    fmt,
    spdlog,
    imgui;
}
```

### Formatter Directives

**Disable formatting for sections:**

```css
// kumi-fmt: off
target messy {
  type:executable;sources:"a.cpp";
}
// kumi-fmt: on
```

**Implementation:**
- Lexer preserves comments as `COMMENT` tokens
- Formatter checks for `kumi-fmt: off/on` directives
- Skip formatting for disabled sections

### CLI Usage

```bash
# Format all files in project
kumi fmt

# Format specific file
kumi fmt kumi.css

# Check formatting without modifying
# Will return 0 if formatted, 1 if not
kumi fmt --check

# Override line width
kumi fmt --line-width 120

# Create default .kumi-format file
kumi fmt init
```

---

## Project Structure

### Recommended Layout

```
kumi/
├── src/
│   ├── lex/           # Lexer
│   ├── parse/         # Parser
│   ├── semantic/      # Semantic analysis
│   ├── build/         # Build system
│   ├── tools/         # Tooling (LSP, formatter)
│   │   ├── lsp/       # LSP server implementation
│   │   └── fmt/       # Formatter implementation
│   └── main.cpp       # Entry point (dispatch to subcommands)
├── docs/
│   └── DOCS.md        # Language reference
└── tests/
    ├── lex/
    ├── parse/
    └── tools/
```

### Code Organization

**`src/tools/lsp/`:**
```
lsp/
├── server.hpp         # LSP server main loop
├── handlers.hpp       # Request handlers (completion, hover, etc.)
├── workspace.hpp      # Workspace management
└── diagnostics.hpp    # Error reporting
```

**`src/tools/fmt/`:**
```
fmt/
├── formatter.hpp      # Main formatter
├── printer.hpp        # Pretty-printer
├── config.hpp         # Configuration parsing
└── directives.hpp     # Handle kumi-fmt: off/on
```
