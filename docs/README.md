# Kumi

Kumi is a declarative build system **and** package manager for C and C++, with
its own small configuration language (`.kumi` files). The goal: define a project
the way you'd describe it — not script it — and get a built-in package manager
and Rust-quality diagnostics on top.

```kumi
project myapp {
    version: "1.0.0";
    standard: "c++23";
}

target myapp {
    type: executable;
    sources: ["src/main.cpp", "src/util.cpp"];
}

dependencies {
    fmt: "10.1.0";
    spdlog: git("https://github.com/gabime/spdlog.git") { tag: "v1.12.0"; };
}
```

> **Status.** Kumi is under active development (rewritten in Rust as a Cargo
> workspace of `kumi-*` crates). What works today: the lexer, parser, semantic
> checker (single-file), and the structural workspace resolver. Planned/stubbed:
> cross-file scope inheritance, the dependency resolver, the build executor, the
> LSP, and the formatter. Sections below mark what is **planned** where it isn't
> yet implemented.

The formal grammar lives in [`kumi.ebnf`](./kumi.ebnf); this document is the
prose reference and design spec.

---

## Contents

- [Design goals](#design-goals)
- [CLI](#cli)
- [Project structure & workspaces](#project-structure--workspaces)
- [Language reference](#language-reference)
- [Semantics](#semantics)
- [Diagnostics](#diagnostics)
- [Dependencies & package management](#dependencies--package-management)
- [Build execution](#build-execution)
- [Tooling](#tooling)
- [Roadmap](#roadmap)

---

## Design goals

- **Declarative, readable.** A `.kumi` file describes _what_ the project is. No
  imperative scripting, no string-typed variables.
- **Built-in package manager.** `kumi add fmt` — no vcpkg/Conan/FetchContent.
- **Excellent diagnostics.** Rust-style errors with source spans, secondary
  "first defined here" labels, and a clear error/warning distinction.
- **Fast.** Native, incremental, designed around a zero-copy arena AST.
- **Cargo-like CLI.** `init`, `add`, `build`, `run`, `check`, `fmt`.

---

## CLI

| Command                 | Purpose                                  | Status      |
| ----------------------- | ---------------------------------------- | ----------- |
| `kumi init`             | Scaffold a new project (interactive)     | implemented |
| `kumi check [file]`     | Parse + semantic-check without building  | implemented |
| `kumi build [target]`   | Build (`--release`, `-j N`, `--verbose`) | planned     |
| `kumi run [target]`     | Build then run                           | planned     |
| `kumi clean [--all]`    | Remove build artifacts                   | planned     |
| `kumi add <pkg>`        | Add a dependency                         | planned     |
| `kumi update [pkg]`     | Update dependencies                      | planned     |
| `kumi remove <pkg>`     | Remove a dependency                      | planned     |
| `kumi search <query>`   | Search the registry                      | planned     |
| `kumi fmt [--check]`    | Format `.kumi` files                     | planned     |
| `kumi serve [--port N]` | Run the LSP server (stdio or TCP)        | planned     |

---

## Project structure & workspaces

### One file per directory

A project is a **tree of directories**, each containing exactly one
`build.kumi`. `members` reference **directories**, and the workspace tree mirrors
the filesystem:

```
myapp/
  build.kumi          # project myapp { } + workspace { members: "lib", "app"; }
  lib/
    build.kumi        # target lib { }
  app/
    build.kumi        # target app { depends: lib; }
```

One `build.kumi` may declare _many_ targets — you add a directory when you want a
new package, not a new target (same model as Bazel `BUILD`, CMake
`CMakeLists.txt`, Cargo `Cargo.toml`).

- The **root** `build.kumi` is the one holding the single `project { }`.
- A file declares its children with `workspace { members: "lib", "tools/gen"; }`.
- Members must be in the **same directory or a subdirectory** — no `../`
  (upward references are rejected). This keeps the tree relocatable and makes
  cross-directory cycles structurally impossible.
- No file may appear twice in the resolved tree (duplicate inclusion is an
  error).

### Scope inheritance (intra-project)

Scope flows **strictly downward**: a file sees symbols from itself and every
ancestor up to the root. A parent never sees its children's symbols; siblings
never see each other's.

| Construct            | Visibility                          | Notes                                                        |
| -------------------- | ----------------------------------- | ------------------------------------------------------------ |
| `mixin`              | inherited downward                  | applied with `with`; the way to share build config           |
| `options`            | inherited downward                  | read in `@if`; a child may not redefine an ancestor's option |
| `project` properties | visible everywhere                  | exactly one `project`, in the root only                      |
| `target`             | **workspace-global** flat namespace | unique names; any target may `depends:` any other            |
| `profile`            | **root-only**, workspace-global     | a profile is a build-wide mode selected at invocation        |

There is **no implicit property cascade**: to push shared build properties
(flags, defines, `standard`, warnings…) down to targets, bundle them in a
`mixin` and opt targets in with `with`. (This is explicit and greppable by
design.)

### Composing projects (inter-project / monorepos)

Two distinct mechanisms — don't confuse them:

|          | Intra-project (`workspace { members }`)  | Inter-project (`dependencies`)                          |
| -------- | ---------------------------------------- | ------------------------------------------------------- |
| Unit     | **one** project across many files        | **separate** projects                                   |
| Scope    | shared downward (mixins/options/project) | isolated — only the dep's export surface                |
| Paths    | same dir / children, no `../`            | anywhere: `path("../x")`, `git(...)`, registry          |
| Identity | one `project { }`                        | each dep has its own `build.kumi` + `project` + version |

A **monorepo** is many self-contained projects composed via `path()`
dependencies:

```
monorepo/
  libs/foo/build.kumi    # project foo { }   target foo { public { include-dirs: "include"; } }
  apps/app/build.kumi    # project app { }   dependencies { foo: path("../../libs/foo"); }
```

When `app` depends on `foo`, it consumes only `foo`'s **export surface** — the
`public`/`interface` visibility blocks of `foo`'s targets — never `foo`'s
internals, mixins, or options. This keeps hard project boundaries (like Bazel
packages) instead of CMake's `add_subdirectory` scope merging.

> There is **no `@import`**. File inclusion is the workspace tree (intra) or
> dependencies (inter) — never arbitrary file imports.

---

## Language reference

The complete formal grammar is [`kumi.ebnf`](./kumi.ebnf). Highlights below.

### Lexical

- **Comments:** `// line` and `/* block */`. `///` is reserved for doc comments
  (consumed by the LSP/hover).
- **Identifiers:** start with a letter or `_`, then letters/digits/`_`/`-`
  (hyphens allowed, e.g. `include-dirs`).
- **Literals:** strings `"..."`, integers, booleans (`true`/`false`).
- **String interpolation:** `"${identifier}"` — a bare identifier only (loop
  var, then builtin variable, then option). Single-pass; `\${` is a literal.
- **Redundant `;`** is tolerated as an empty statement anywhere (it produces a
  _warning_, not an error).

### Declarations

```kumi
project myapp {                 // exactly one, in the root build.kumi
    version: "1.0.0";
    standard: "c++23";
    license: "MIT";
}

workspace {                     // declares child directories
    members: "lib", "app";
}

target mylib with strict {      // build output; composes mixins via `with`
    type: "static_library";
    public  { include-dirs: "include"; }
    private { sources: ["src/lib.cpp"]; }
}

mixin strict {                  // reusable property bundle, inherited downward
    warnings: "all";
    werror: true;
}

profile release with strict {   // build-wide mode, root-only
    optimize: "3";
    lto: true;
}

dependencies {
    fmt: "10.1.0";
    spdlog: git("https://github.com/gabime/spdlog.git") { tag: "v1.12.0"; };
    vulkan?: system();          // `?` = optional dependency
}

options {
    ENABLE_TESTS: true { description: "Build the test suite"; };
    LOG_LEVEL: "info";
}

install { bin-dir: "bin"; lib-dir: "lib"; }
package { name: "myapp"; license: "MIT"; }
script  { name: "prebuild"; command: "python gen.py"; phase: "prebuild"; }
```

### Visibility blocks

Inside a `target` (or `mixin`), `public` / `private` / `interface` control how
properties propagate to consumers (the C/C++ usage-requirement model). `public`
and `interface` form the target's export surface to dependents.

### Control flow

```kumi
@if platform() == "linux" {
    defines: "PLATFORM_LINUX";
} @else-if platform() == "windows" {
    defines: "PLATFORM_WIN32";
} @else {
    defines: "PLATFORM_OTHER";
}

@for file in glob("src/*.cpp") {
    @if file == "src/deprecated.cpp" { @continue; }
    sources: file;
}
```

`@break` / `@continue` are valid only inside `@for`. Code after them in the same
block is unreachable (a warning).

### Properties & values

Values are comma-separated, or written as an explicit bracketed list — the two
forms are equivalent:

```kumi
sources: "main.cpp", "util.cpp";
sources: ["main.cpp", "util.cpp"];   // identical
```

### Conditions

```kumi
@if platform == "linux" and arch != "arm" { }
@if not has_feature("avx2") { }
@if (debug or sanitize) and not release { }
```

### Diagnostic directives

`@error`, `@warning`, `@info`, `@debug` emit messages during evaluation:

```kumi
@if not has_dep("vulkan") { @error "vulkan is required on this platform"; }
```

---

## Semantics

### Property merge

- **Scalar** properties: one definition per resolved scope (including mixin
  composition). Conflict → error.
- **List** properties: appended from all sources. List properties include:
  `sources`, `defines`, `include-dirs`, `system-include-dirs`, `link-libraries`,
  `link-dirs`, `compile-options`, `link-options`, `headers`, `frameworks`,
  `sanitizers`.

### Validation rules

- Duplicate `target`/`mixin`/`profile`/`option` definitions are errors (with a
  "first defined here" label).
- The same source file listed twice within a scope is an error.
- Option names must be `UPPER_SNAKE_CASE` and must not shadow a builtin variable.
- `@break`/`@continue` outside a loop, and unreachable code after them, are
  reported.
- Mixin references in `with` must resolve; conflicting scalar properties across
  composed mixins are errors.

### Builtin variables

Always in scope; cannot be shadowed by options.

| Name                | Type   | Meaning                     |
| ------------------- | ------ | --------------------------- |
| `platform`          | string | `linux`, `windows`, `macos` |
| `arch`              | string | `x86_64`, `arm64`           |
| `config`            | string | `debug`, `release`          |
| `debug` / `release` | bool   | current configuration       |
| `sanitize`          | bool   | sanitizers enabled          |

### Builtin functions

| Name                                 | Params | Returns | Context    |
| ------------------------------------ | ------ | ------- | ---------- |
| `platform()` / `arch()` / `config()` | 0      | string  | condition  |
| `has_feature(name)`                  | 1      | bool    | condition  |
| `has_dep(name)`                      | 1      | bool    | condition  |
| `option(NAME)`                       | 1      | varies  | condition  |
| `files(glob)` / `glob(glob)`         | 1      | list    | iterable   |
| `git(url)` / `path(path)`            | 1      | dep     | dependency |
| `system()`                           | 0      | dep     | dependency |

### Checker evaluation order

1. Resolve the workspace tree (paths, duplicates, cycles).
2. Build the scope chain (root → leaves).
3. Collect: register named symbols, detect duplicates, validate option names.
4. Validate: resolve references, check properties against the registry,
   type-check conditions, check interpolations, validate dependencies, enforce
   structural rules — emitting diagnostics in resolution order.

---

## Diagnostics

Diagnostics carry a **severity** (`error` / `warning`). Errors fail the build
(non-zero exit); warnings print and the build continues. Rendering uses
`codespan-reporting` with a primary underline, optional secondary label, and a
help note:

```
error: duplicate target definition 'myapp'
   |
8  | target myapp {
   | ^^^^^^^^^^^^ first defined here
   :
15 | target myapp {
   | ^^^^^^^^^^^^ duplicate definition
   |
   = help: each target must have a unique name
```

The underline is clamped to a construct's header (e.g. `target myapp`, not its
whole body). Warnings include unreachable code after `@break`/`@continue` and
redundant `;`.

---

## Dependencies & package management

Dependency values:

- **String** — a version specifier (`fmt: "10.1.0";`).
- **`git(url)`** — fetch from git, with optional `tag` / `branch` / `commit`.
- **`path(p)`** — a local path dependency (the monorepo composition mechanism).
- **`system()`** — resolve via the system package manager / pkg-config.

`name?:` marks a dependency optional — if it can't be resolved the build doesn't
fail, and `has_dep("name")` returns false for use in conditions. Multiple
`dependencies` blocks merge; the same dependency declared twice with conflicting
versions is an error, with the same version a warning.

> **Planned.** The resolver, lockfile, and registry are not yet implemented.

---

## Build execution

> **Planned / roadmap.** The build executor is not yet implemented.

The strategy is to **bundle Ninja first** (generate `.ninja` files from the
build graph and shell out) to reach working builds quickly, then replace it with
a **native parallel executor** with content-addressed caching (keyed on source +
flags + dependency fingerprints, not timestamps). The build graph provides
topological ordering for correct parallel scheduling.

---

## Tooling

### LSP (`kumi serve`) — planned

Built into the main binary; shares the lexer/parser/checker, so it never drifts
from the compiler. Planned features: live diagnostics, completion, go-to-
definition (mixins/dependencies), hover from `///` doc comments, document
symbols, and semantic-token highlighting. Parses on change (not save).

### Formatter (`kumi fmt`) — planned

Opinionated, minimal config. Optional `.kumi-format` (`line-width`, `indent`,
`wrap-lists`); `// kumi-fmt: off` / `on` disables a region. `kumi fmt --check`
exits non-zero if reformatting is needed.

### Editor grammars

The LSP is necessary but not sufficient for highlighting (latency; not every
context runs an LSP). Plan:

- **TextMate grammar** (`.tmLanguage.json`) inside a VS Code extension —
  always-on baseline highlighting for VS Code/Sublime.
- **LSP semantic tokens** — cheap given the typed lexer; refines highlighting in
  any LSP client.
- **tree-sitter grammar** — for Neovim/Helix/Zed and eventual GitHub
  highlighting; translated from [`kumi.ebnf`](./kumi.ebnf).

During development these live in-repo under `editors/`; the tree-sitter grammar
is extracted to a standalone `tree-sitter-kumi` repo once the language settles.

---

## Roadmap

| Area                                                 | Status  |
| ---------------------------------------------------- | ------- |
| Lexer, parser, AST (arena, zero-copy)                | done    |
| Semantic checker (single file)                       | done    |
| Diagnostics (severity, spans, labels)                | done    |
| Workspace resolver (structural: tree, paths, cycles) | done    |
| Cross-file scope inheritance                         | planned |
| Dependency resolver + lockfile + registry            | planned |
| Build executor (bundled Ninja → native)              | planned |
| LSP + formatter                                      | planned |
| Editor grammars (TextMate, tree-sitter)              | planned |
