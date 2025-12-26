# Kumi Complete Keywords Reference

## Top-Level Blocks

### `project`

Defines project metadata.

```css
project myapp {
  version: "1.0.0";
  language: cpp23;
  authors: "dev@example.com";
  license: "MIT";
  description: "My awesome project";
}
```

### `workspace`

Defines a multi-project workspace.

```css
workspace {
  members: core, renderer, physics;
  members: tools; /*;  /* Glob pattern */
}
```

### `dependencies` (or `deps`)

Declares project dependencies.

```css
dependencies {
  fmt: "^10.2.1";
  spdlog: "~1.12";
  boost: "1.84";
}
```

### `target`

Defines a build target (executable, library, etc.).

```css
target myapp {
  type: executable;
  sources: "src/**/*.cpp";
}
```

### `options`

Defines build options (user-configurable).

```css
options {
  build_tests: bool = true;
  max_threads: int = 8;
  install_prefix: path = "/usr/local";
}
```

### `global`

Global settings applied to all targets.

```css
global {
  cpp: 23;
  warnings: all;
  optimize: speed;
}
```

### `mixin`

Defines reusable configuration blocks.

```css
mixin strict-warnings {
  warnings: all;
  warnings-as-errors: true;
}
```

### `preset`

Defines predefined configuration presets.

```css
preset embedded {
  optimize: size;
  no-exceptions: true;
  no-rtti: true;
}
```

### `features`

Defines optional features.

```css
features {
  networking {
    default: false;
    deps: asio;
    defines: HAS_NETWORKING;
  }
}
```

### `testing`

Configures testing framework.

```css
testing {
  framework: catch2;
  timeout: 30s;
  parallel: 4;
}
```

### `install`

Defines installation rules.

```css
install {
  targets: myapp, mylib;
  destination {
    executables: bin;
    libraries: lib;
  }
}
```

### `package`

Defines packaging configuration.

```css
package {
  name: "MyApp";
  generators: deb, rpm, zip;
}
```

### `scripts`

Defines custom build scripts.

```css
scripts {
  prebuild {
    command: "python generate.py";
    inputs: "templates/**/*";
    outputs: "gen/**/*";
  }
}
```

### `toolchain`

Defines custom toolchain.

```css
toolchain custom-arm {
  compiler {
    c: "arm-gcc";
    cpp: "arm-g++";
  }
  sysroot: "/opt/arm-sysroot";
}
```

### `:root`

Defines global variables (CSS custom properties style).

```css
:root {
  --version: "1.0.0";
  --build-dir: "build";
}
```

---

## Target Properties

### Type Properties

```css
target myapp {
  type: executable; /* executable, static-lib, shared-lib, */
  /* interface, object-lib, plugin, test, */
  /* benchmark, custom */
}
```

### Source Properties

```css
target myapp {
  sources: "src/**/*.cpp"; /* Source files */
  headers: "include/**/*.hpp"; /* Header files */
  modules: "modules/**/*.cppm"; /* C++20 modules */
  resources: "assets/**/*"; /* Resource files */
}
```

### Dependency Properties

```css
target myapp {
  depends: mylib, fmt, spdlog; /* Dependencies */
  depends-private: internal-lib; /* Private deps */
  depends-interface: header-only; /* Interface deps */
}
```

### Compilation Properties

```css
target myapp {
  cpp: 23; /* C++ standard */
  c: 17; /* C standard */

  compile-options:
    -Wall,
    -Wextra; /* Compiler flags */
  compile-features: cxx_std_23; /* Compile features */
  defines:
    DEBUG,
    VERSION= "1.0"; /* Preprocessor defines */

  include-dirs: "include"; /* Include directories */
  system-include-dirs: "/opt/include"; /* System includes */
}
```

### Linking Properties

```css
target myapp {
  link-options: -pthread; /* Linker flags */
  link-libraries: pthread, dl, m; /* Link libraries */
  link-dirs: "/usr/local/lib"; /* Library search paths */

  frameworks: Cocoa, Metal; /* macOS frameworks */
}
```

### Optimization Properties

```css
target myapp {
  optimize: none; /* none, debug, speed, size, aggressive */
  lto: true; /* Link-time optimization */
  lto-mode: full; /* full, thin */
  strip: true; /* Strip symbols */
  debug-info: full; /* none, line-tables, full */
}
```

### Warning Properties

```css
target myapp {
    warnings: all;               /* none, basic, all, strict, pedantic */
    warnings-as-errors: true;    /* Treat warnings as errors */

    /* Individual warnings */
    warnings: strict {
        cast-align;
        conversion;
        shadow: all;
    }
}
```

### Sanitizer Properties

```css
target myapp {
  sanitizers: address, undefined; /* address, thread, memory, */
  /* undefined, leak */
}
```

### Other Properties

```css
target myapp {
  output-name: "myapp-v2"; /* Custom output name */
  output-dir: "bin"; /* Output directory */
  version: "1.2.3"; /* Library version (SO) */

  position-independent: true; /* -fPIC */
  visibility: hidden; /* Symbol visibility */

  no-exceptions: false; /* Disable exceptions */
  no-rtti: false; /* Disable RTTI */

  precompiled-headers: "pch.hpp"; /* PCH file */
  unity-build: true; /* Unity/jumbo build */

  working-directory: "tests"; /* Working dir for tests */
  timeout: 60s; /* Test timeout */
}
```

### Visibility Scopes

```css
target myapp {
  public {
    /* Visible to dependents */
    include-dirs: "include";
    defines: API_VERSION=1;
  }

  private {
    /* Only for this target */
    defines: INTERNAL_BUILD;
  }

  interface {
    /* Header-only, no compilation */
    compile-features: cxx_std_23;
  }
}
```

---

## Control Flow Keywords

### `@if` / `@else-if` / `@else`

Conditional compilation.

```css
@if platform(windows) {
  /* ... */
}
@else-if platform(macos) {
  /* ... */
} @else  {
  /* ... */
}
```

### `@for`

Loop iteration.

```css
@for item in list {
  /* ... */
}

@for i in 0.10 {
  /* ... */
}
```

### `@break`

Break out of loop.

```css
@for file in glob("*.cpp") {
  @if file.contains("old") {
    @break;
  }
}
```

### `@continue`

Skip to next iteration.

```css
@for file in glob("*.cpp") {
  @if file.contains("test") {
    @continue;
  }
}
```

### `@error`

Emit compile-time error.

```css
@if not platform(linux, windows, macos) {
  @error "Unsupported platform";
}
```

### `@warning`

Emit compile-time warning.

```css
@if cpp-version < 20 {
  @warning "C++20 or later recommended";
}
```

### `@apply`

Apply preset/profile.

```css
@apply profile(release-optimized) {
  target myapp {
    /* ... */
  }
}
```

---

## Conditional Functions

### Platform Checks

```css
platform(windows)              /* Windows */
platform(linux)                /* Linux */
platform(macos)                /* macOS */
platform(unix)                 /* Any Unix-like */
platform(ios)                  /* iOS */
platform(android)              /* Android */
platform(wasm)                 /* WebAssembly */
```

### Architecture Checks

```css
arch(x86_64)                   /* x86-64 */
arch(x86)                      /* x86 32-bit */
arch(arm64)                    /* ARM 64-bit */
arch(arm)                      /* ARM 32-bit */
arch(wasm32)                   /* WebAssembly 32-bit */
arch(riscv64)                  /* RISC-V 64-bit */
```

### Compiler Checks

```css
compiler(gcc)                  /* GCC */
compiler(clang)                /* Clang */
compiler(msvc)                 /* MSVC */
compiler(intel)                /* Intel C++ */
```

### Configuration Checks

```css
config(debug)                  /* Debug build */
config(release)                /* Release build */
config(release-with-debug)     /* Release + debug info */
config(min-size-release)       /* Optimize for size */
```

### Feature Checks

```css
feature(enable-logging)        /* Custom feature enabled */
has-feature(avx2)              /* CPU feature available */
has-feature(neon)              /* ARM NEON available */
```

### Version Checks

```css
cpp-version >= 23              /* C++ version */
cpp-version == 20
c-version >= 17                /* C version */
```

### Option Checks

```css
option(BUILD_TESTS)            /* Check if option true */
option(ENABLE_NETWORKING)
```

### Existence Checks

```css
exists("vendor/lib")           /* File/directory exists */
exists("config.json")
```

### Logical Operators

```css
platform(windows) and arch(x86_64)
platform(linux) or platform(macos)
not platform(windows)
(condition1 and condition2) or condition3
```

---

## Functions & Built-ins

### Variable Functions

```css
var(--version)                 /* Get variable value */
${variable}                    /* String interpolation */
```

### Path Functions

```css
glob("src/**/*.cpp")           /* Glob pattern matching */
file.stem                      /* Filename without extension */
file.extension                 /* File extension */
file.directory                 /* Parent directory */
```

### String Functions

```css
file.contains("test")          /* String contains check */
string.starts-with("prefix")   /* Starts with check */
string.ends-with(".cpp")       /* Ends with check */
```

### Dependency Functions

```css
git("https://github.com/user/repo") {
  tag: "v1.0";
  branch: "main";
  commit: "abc123";
}

url("https://example.com/lib.tar.gz") {
  sha256: "hash...";
}

system {
  required: true;
  min-version: "1.0";
}
```

---

## Special Keywords

### `apply`

Apply mixin to target.

```css
target myapp {
  apply: strict-warnings;
}
```

### `inherits`

Inherit from another target/mixin.

```css
target myapp {
  inherits: base-config;
}
```

### `extends`

Extend another definition (alias for inherits).

```css
target myapp extends base-target {
  /* ... */
}
```

### `use-preset`

Use a predefined preset.

```css
target myapp {
  use-preset: embedded;
}
```

### `export`

Mark target for export (install/packaging).

```css
target mylib {
  export: true;
  export {
    cmake-config: true;
    pkg-config: true;
  }
}
```

### `import`

Import module (C++20 modules).

```css
target myapp {
  import: mylib; /* Import module */
}
```

---

## Reserved for Future Use

```css
/* Potentially reserved keywords */
namespace       /* Module namespacing */
component       /* Component-based builds */
generator       /* Code generation */
cache           /* Explicit cache control */
remote          /* Remote build config */
distributed     /* Distributed builds */
plugin          /* Plugin system */
hook            /* Build hooks */
validator       /* Custom validators */
transformer     /* Custom transformers */
```

---

## Context-Specific Keywords

### In `dependencies` block:

```css
dependencies {
  fmt: "10.2.1"; /* version string */
  boost: latest; /* special: latest */
  mylib?: "1.0"; /* optional (?) */

  boost: "1.84" {
    components: system, thread;
    static: true;
    header-only: false;
  }
}
```

### In `options` block:

```css
options {
  MY_OPTION: bool = true {
    description: "Enable feature";
  }

  LEVEL: choice = "O2" {
    values: "O0", "O1", "O2", "O3";
  }

  COUNT: int = 8 {
    min: 1;
    max: 128;
  }

  path: path = "/usr/local";
  string: string = "default";
}
```

### In `testing` block:

```css
testing {
  framework: catch2; /* catch2, gtest, doctest */
  timeout: 30s;
  parallel: 4;
  auto-discover: "tests/**/*_test.cpp";
}
```

### In `install` block:

```css
install {
    destination {
        executables: bin;
        libraries: lib;
        headers: include;
        resources: share;
        documentation: doc;
    }

    component runtime { ... }
    component development { ... }
}
```

---

## Operator Keywords

```css
and            /* Logical AND */
or             /* Logical OR */
not            /* Logical NOT */
in             /* List membership */
```

---

## Value Keywords (Context-dependent)

### Boolean Values

```css
true
false
```

### Build Types

```css
executable
static-lib
shared-lib
interface
object-lib
plugin
module-lib
test
benchmark
custom
```

### Optimization Levels

```css
none
debug
speed
size
aggressive
```

### Warning Levels

```css
none
basic
all
strict
pedantic
```

### Sanitizers

```css
address
thread
memory
undefined
leak
```

### Frameworks (macOS)

```css
Cocoa
Metal
IOKit
CoreFoundation
/* ... etc */
```

### Test Frameworks

```css
catch2
gtest
doctest
google-benchmark
```

### Package Generators

```css
deb
rpm
zip
tar.gz
nsis
dmg
```

---

## Total Keyword Count: ~120+

**Categories:**

- Top-level blocks: 15
- Target properties: 40+
- Control flow: 8
- Conditional functions: 20+
- Built-in functions: 10+
- Special keywords: 10
- Value keywords: 20+
- Reserved: 10+

**Note:** Many are context-specific (only valid in certain blocks)
