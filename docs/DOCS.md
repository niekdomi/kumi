# Kumi Language Reference

---

## Table of Contents

1. [Introduction](#introduction)
2. [Lexical Structure](#lexical-structure)
3. [Top-Level Declarations](#top-level-declarations)
4. [Target Configuration](#target-configuration)
5. [Control Flow](#control-flow)
6. [Built-in Functions](#built-in-functions)
7. [Expressions and Values](#expressions-and-values)
8. [Complete Grammar Reference](#complete-grammar-reference)
9. [CMake Migration Guide](#cmake-migration-guide)

---

## Introduction

Kumi is a modern build system for C++ with a clean, CSS-like syntax. It
combines the simplicity of declarative configuration with the power of
conditional compilation and loops.

### Design Principles

- **Declarative over imperative** - Describe what to build, not how
- **Convention over configuration** - Sensible defaults, minimal boilerplate
- **One obvious way** - Clear, idiomatic patterns
- **Type-safe and fast** - Catch errors early, build quickly

### Hello World

```css
project hello {
  version: "1.0.0";
}

target hello {
  type: executable;
  sources: "main.cpp";
}
```

Build and run:

```bash
kumi build
kumi run hello
```

---

## Lexical Structure

### Comments

```css
// Line comment

/*
 * Block comment
 * Can span multiple lines
 */
```

### Identifiers

```css
myapp          // Valid
my-app         // Valid (kebab-case)
my_app         // Valid (snake_case)
MyApp          // Valid (PascalCase)
123invalid     // Invalid (cannot start with digit)
```

### Literals

```css
"string literal"    // String
123                 // Integer (positive only)
true                // Boolean
false               // Boolean
```

### String Interpolation

```css
project myapp {
    version: "1.0.0";
}

target myapp {
    output-name: "myapp-${project.version}";  // → "myapp-1.0.0"
}
```

**Supported interpolations:**

- `${project.name}` - Project name
- `${project.version}` - Project version
- `${variable}` - Loop variable in `@for`

---

## Top-Level Declarations

Top-level declarations can only appear at the root level of a `.kumi` file.

### project

**Syntax:**

```ebnf
ProjectDecl = "project" Identifier "{" { PropertyAssignment } "}" ;
```

**Purpose:** Defines project metadata

**Context:** Root level only, required in every project

**Properties:**

- `version: String` - Project version (semantic versioning recommended)
- `authors: String | List<String>` - Project authors
- `license: String` - License identifier (e.g., "MIT", "Apache-2.0")
- `description: String` - Project description

**Example:**

```css
project myengine {
  version: "2.1.0";
  authors: "John Doe <john@example.com>";
  license: "MIT";
  description: "A high-performance game engine";
}
```

**CMake equivalent:**

```cmake
project(myengine VERSION 2.1.0)
set(PROJECT_DESCRIPTION "A high-performance game engine")
```

---

### workspace

**Syntax:**

```ebnf
WorkspaceDecl = "workspace" "{" { PropertyAssignment } "}" ;
```

**Purpose:** Defines a multi-project workspace (monorepo)

**Context:** Root level of workspace root file only

**Properties:**

- `members: List<String>` - List of workspace member directories

**Example:**

```css
workspace {
  members: "core", "renderer", "physics", "tools/editor";
}
```

**Structure:**

```
my-engine/
├── kumi.css              # Workspace root
├── core/
│   └── kumi.css          # Project 1
├── renderer/
│   └── kumi.css          # Project 2
├── physics/
│   └── build.kumi        # Project 3
└── tools/
    └── editor/
        └── build.kumi    # Project 4
```

**CMake equivalent:**

```cmake
# Root CMakeLists.txt
add_subdirectory(core)
add_subdirectory(renderer)
add_subdirectory(physics)
add_subdirectory(tools/editor)
```

---

### target

**Syntax:**

```ebnf
TargetDecl = "target" Identifier [ "with" IdentifierList ] "{" { TargetContent } "}" ;
TargetContent = PropertyAssignment | VisibilityBlock | Statement ;
IdentifierList = Identifier { "," Identifier } ;
```

**Purpose:** Defines a build target (executable, library, test, ...)

**Context:**

- Root level
- Inside `@if`, `@for` blocks
- Inside other targets (nested targets)

**Common Properties:**

- `type: Identifier` - Target type (see [Target Types](#target-types))
- `sources: String | List<String>` - Source files (supports globs)
- `headers: String | List<String>` - Header files
- `depends: List<Identifier>` - Dependencies (internal or external)
- `compile-options: List<String>` - Compiler flags
- `defines: List<String>` - Preprocessor definitions
- `include-dirs: List<String>` - Include directories
- `link-libraries: List<String>` - Libraries to link
- `link-options: List<String>` - Linker flags
- `optimize: Identifier` - Optimization level (none, debug, speed, size, aggressive)
- `warnings: Identifier` - Warning level (none, basic, all, strict, pedantic)
- `warnings-as-errors: Boolean` - Treat warnings as errors

**Example:**

```css
target myapp {
  type: executable;
  sources: "src/**/*.cpp";
  headers: "include/**/*.hpp";
  depends: mylib, fmt, spdlog;

  compile-options: "-Wall", "-Wextra";
  defines:
    APP_VERSION= "1.0",
    DEBUG_MODE;
  optimize: speed;
  warnings: strict;
  warnings-as-errors: true;
}
```

**CMake equivalent:**

```cmake
add_executable(myapp src/main.cpp)
target_sources(myapp PRIVATE ${SOURCES})
target_link_libraries(myapp PRIVATE mylib fmt spdlog)
target_compile_options(myapp PRIVATE -Wall -Wextra)
target_compile_definitions(myapp PRIVATE APP_VERSION="1.0" DEBUG_MODE)
set_target_properties(myapp PROPERTIES
    CXX_STANDARD 23
    COMPILE_WARNING_AS_ERROR ON
)
```

#### Target Types

| Type         | Description                        | CMake Equivalent                  |
| ------------ | ---------------------------------- | --------------------------------- |
| `executable` | Executable program                 | `add_executable()`                |
| `static-lib` | Static library (.a, .lib)          | `add_library(... STATIC)`         |
| `shared-lib` | Shared library (.so, .dll, .dylib) | `add_library(... SHARED)`         |
| `interface`  | Header-only library                | `add_library(... INTERFACE)`      |
| `object-lib` | Object file library                | `add_library(... OBJECT)`         |
| `test`       | Test executable                    | `add_executable()` + `add_test()` |
| `benchmark`  | Benchmark executable               | `add_executable()`                |

---

### dependencies

**Syntax:**

```ebnf
DependenciesDecl = "dependencies" "{" { DependencySpec } "}" ;
DependencySpec = Identifier [ "?" ] ":" DependencyValue [ DependencyOptions ] ";" ;
```

**Purpose:** Declares external dependencies

**Context:** Root level, inside `global` block

**Dependency Types:**

- `String` - Version from registry
- `system` - System package
- `git(url)` - Git repository
- `path(path)` - Local path

**Optional Dependencies:**
Use `?` suffix to mark as optional

**Example:**

```css
dependencies {
    // Registry dependencies
    fmt: "^10.2.1";
    spdlog: "1.12.0";

    // Optional dependencies
    asio?: "1.28.0";

    // Git dependencies
    imgui: git("https://github.com/ocornut/imgui") {
        tag: "v1.90";
    };

    // System dependencies
    opengl?: system;
    vulkan: system {
        min-version: "1.3";
    };

    // Local path
    mylib: path("../mylib");
}
```

**CMake equivalent:**

```cmake
find_package(fmt 10.2.1 REQUIRED)
find_package(spdlog 1.12.0 REQUIRED)
find_package(OpenGL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.90
)
FetchContent_MakeAvailable(imgui)
```

---

### options

**Syntax:**

```ebnf
OptionsDecl = "options" "{" { OptionSpec } "}" ;
OptionSpec = Identifier ":" Value [ OptionConstraints ] ";" ;
```

**Purpose:** Defines user-configurable build options

**Context:** Root level only

**Example:**

```css
options {
  build_tests: true;
  build_benchmarks: false;
  MAX_THREADS: 8 {
    min: 1;
    max: 128;
  }
  install_prefix: "/usr/local";
  LOG_LEVEL: "info" {
    choices: "debug", "info", "warning", "error";
  }
}
```

**Usage in conditionals:**

```css
@if option(BUILD_TESTS) {
  target tests {
    type: test;
    sources: "tests/**/*.cpp";
  }
}
```

**Command line:**

```bash
kumi build -o BUILD_TESTS=false -o MAX_THREADS=16
```

**CMake equivalent:**

```cmake
option(BUILD_TESTS "Build tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
set(MAX_THREADS 8 CACHE STRING "Maximum threads")
```

---

### mixin

**Syntax:**

```ebnf
MixinDecl = "mixin" Identifier "{" { PropertyAssignment | VisibilityBlock } "}" ;
```

**Purpose:** Defines reusable property sets

**Context:** Root level only

**Example:**

```css
mixin strict-warnings {
  warnings: all;
  warnings-as-errors: true;
  compile-options: "-Wpedantic -Wextra -Wshadow";
}

mixin embedded-target {
  optimize: size;
  no-exceptions: true;
  no-rtti: true;

  public {
    cpp: 23;
  }
}

// Apply mixins using 'with' keyword
target myapp with strict-warnings {
  type: executable;
  sources: "src/**/*.cpp";
}

target firmware with embedded-target,
strict-warnings {
  type: executable;
  sources: "firmware/**/*.cpp";
}
```

**CMake equivalent:**

```cmake
# Would require custom function/macro
function(apply_strict_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    set_target_properties(${target} PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endfunction()
```

---

### profile

**Syntax:**

```ebnf
ProfileDecl = "profile" Identifier [ "with" MixinList ] "{" { PropertyAssignment } "}" ;
MixinList   = Identifier { "," Identifier } ;
```

**Purpose:** Defines named build configuration profiles. Profiles can compose mixins for shared toolchain settings.

**Context:** Root level only

**Example:**

```css
// Mixin for shared toolchain configuration
mixin gcc-toolchain {
  compiler: gcc;
  linker: mold;
  cpp: 23;
}

// Profiles can compose mixins
profile debug with gcc-toolchain {
  optimize: none;
  debug-info: full;
  sanitizers: address, undefined;
}

profile release with gcc-toolchain {
  optimize: aggressive;
  lto: full;
  strip: true;
  defines: NDEBUG;
}

// Profile without mixin
profile release-with-debug {
  optimize: speed;
  debug-info: full;
  lto: thin;
}
```

**Usage:**

```bash
kumi build --profile dev
kumi build --profile release
```

**CMake equivalent:**

```cmake
set(CMAKE_BUILD_TYPE Release)
# Or use CMAKE_CXX_FLAGS_RELEASE, etc.
```

---

### @import

**Syntax:**

```ebnf
ImportDecl = "@import" String ";" ;
```

**Purpose:** Imports another Kumi configuration file

Note that the file ending (`.kumi`) can be omitted when importing the file.

**Context:** Root level, inside blocks

**Example:**

**File structure:**

```
project/
├── kumi.css
├── common/
│   ├── mixins.kumi
│   └── warnings.kumi
└── platforms/
    ├── windows.kumi
    └── linux.kumi
```

**kumi.css:**

```css
@import "common/mixins";
@import "common/warnings";

project myapp {
  version: "1.0.0";
}

@if platform(windows) {
  @import "platforms/windows";
}

@if platform(linux) {
  @import "platforms/linux";
}
```

**common/mixins.kumi:**

```css
mixin strict {
  warnings: all;
  warnings-as-errors: true;
}
```

**CMake equivalent:**

```cmake
include(common/mixins.cmake)
include(platforms/windows.cmake)
```

---

## Target Configuration

### Visibility Blocks

**Syntax:**

```ebnf
VisibilityBlock = ( "public" | "private" | "interface" ) "{" { PropertyAssignment } "}" ;
```

**Purpose:** Controls property visibility for dependencies

**Context:** Inside `target` or `mixin` blocks only

**Visibility levels:**

- `public` - Visible to this target and all dependents
- `private` - Visible only to this target
- `interface` - Visible only to dependents (header-only)

**Example:**

```css
target mylib {
    type: static-lib;
    sources: "src/**/*.cpp";

    public {
        include-dirs: "include";
        defines: MYLIB_VERSION=2;
        cpp: 23;
    }

    private {
        include-dirs: "src/internal";
        defines: MYLIB_BUILD;
        compile-options: -fPIC;
    }
}

target myapp {
    type: executable;
    depends: mylib;  // Inherits public properties from mylib
}
```

**CMake equivalent:**

```cmake
add_library(mylib STATIC src/lib.cpp)
target_include_directories(mylib
    PUBLIC include
    PRIVATE src/internal
)
target_compile_definitions(mylib
    PUBLIC MYLIB_VERSION=2
    PRIVATE MYLIB_BUILD
)
```

---

## Control Flow

### Conditionals

**Syntax:**

```ebnf
ConditionalBlock = IfBlock [ ElseBlock ] ;
IfBlock          = "@if" Condition "{" { Statement } "}" ;
ElseBlock        = "@else" ( IfBlock | "{" { Statement } "}" ) ;
```

**Purpose:** Conditional compilation based on platform, configuration, options, etc.

**Context:** Root level, inside targets, inside other conditionals

**Example:**

```css
@if platform(windows) {
  target myapp {
    sources: "src/windows/**/*.cpp";
    link-libraries: ws2_32, user32;
  }
}
@else-if platform(macos) {
  target myapp {
    sources: "src/macos/**/*.cpp";
    frameworks: Cocoa, Metal;
  }
} @else  {
  target myapp {
    sources: "src/linux/**/*.cpp";
    link-libraries: pthread, dl;
  }
}
```

**Inside targets:**

```css
target myapp {
  type: executable;
  sources: "src/common/**/*.cpp";

  @if platform(windows) {
    sources: "src/windows/**/*.cpp";
    defines: PLATFORM_WINDOWS;
  }

  @if feature(networking) {
    depends: asio;
    defines: HAS_NETWORKING;
  }

  @if config(debug) {
    defines: DEBUG_MODE;
    sanitizers: address;
  }
}
```

**CMake equivalent:**

```cmake
if(WIN32)
    target_sources(myapp PRIVATE src/windows/file.cpp)
elseif(APPLE)
    target_sources(myapp PRIVATE src/macos/file.cpp)
else()
    target_sources(myapp PRIVATE src/linux/file.cpp)
endif()
```

---

### Loops

**Syntax:**

```ebnf
ForLoop  = "@for" Identifier "in" Iterable "{" { Statement } "}" ;
Iterable = List | Range | FunctionCall ;
Range    = Number ".." Number ;
```

**Purpose:** Iterate over lists, ranges, or file patterns

**Context:** Root level, inside targets, inside conditionals

**List iteration:**

```css
@for module in [core, renderer, physics, audio] {
    target ${module}-lib {
        type: static-lib;
        sources: "modules/${module}/**/*.cpp";

        public {
            include-dirs: "modules/${module}/include";
        }
    }
}
```

**Range iteration:**

```css
@for worker in 0..7 {
    target worker-${worker} {
        type: executable;
        sources: "worker/main.cpp";
        defines: WORKER_ID=${worker}, WORKER_COUNT=8;
    }
}
```

**File pattern iteration:**

```css
@for file in glob("plugins/*.cpp") {
    target plugin-${file.stem} {
        type: shared-lib;
        sources: ${file};
    }
}
```

**Inside targets:**

```css
target myapp {
  type: executable;
  sources: "src/main.cpp";

  @for lang in [en, de, fr, es, ja] {
    resources: "locales/${lang}/**/*";
  }
}
```

**CMake equivalent:**

```cmake
foreach(module core renderer physics audio)
    add_library(${module}-lib src/${module}.cpp)
endforeach()
```

---

### Loop Control

**Syntax:**

```ebnf
LoopControl = ( "@break" | "@continue" ) ";" ;
```

**Purpose:** Control loop execution

**Context:** Inside `@for` loops only

**Example:**

```css
@for file in glob("src/**/*.cpp") {
    @if file.contains("deprecated") {
        @continue;  // Skip deprecated files
    }

    @if file.contains("experimental") and not option(ENABLE_EXPERIMENTAL) {
        @continue;
    }

    target ${file.stem} {
        sources: ${file};
    }
}
```

---

## Built-in Functions

Built-in functions are used in conditions and expressions.

### Platform Detection

**Function:** `platform(identifier)`

**Purpose:** Detect target platform

**Returns:** Boolean

**Supported platforms:**

- `windows` - Windows (all versions)
- `linux` - Linux
- `macos` - macOS

**Example:**

```css
@if platform(windows) {
    // Windows-specific configuration
}

@if platform(linux) or platform(macos) {
    // Unix-like platforms
}
```

---

### Architecture Detection

**Function:** `arch(identifier)`

**Purpose:** Detect target architecture

**Returns:** Boolean

**Supported architectures:**

- `x86_64` - x86-64 / AMD64
- `x86` - x86 32-bit
- `arm64` - ARM 64-bit (aarch64)
- `arm` - ARM 32-bit

**Example:**

```css
@if arch(x86_64) {
  target myapp {
    compile-options: "-march=native", "-mavx2";
  }
}

@if arch(arm64) {
  target myapp {
    compile-options: "-march=armv8-a";
  }
}
```

---

### Compiler Detection

**Function:** `compiler(identifier)`

**Purpose:** Detect compiler type

**Returns:** Boolean

**Supported compilers:**

- `gcc` - GNU Compiler Collection
- `clang` - Clang/LLVM
- `msvc` - Microsoft Visual C++
- `intel` - Intel C++ Compiler
- `apple-clang` - Apple's Clang

**Example:**

```css
@if compiler(msvc) {
  profile default {
    compile-options: "/W4", "/permissive-";
  }
}

@if compiler(gcc) or compiler(clang) {
  profile default {
    compile-options: "-Wall", "-Wextra", "-Wpedantic";
  }
}
```

---

### Build Configuration

**Function:** `config(identifier)`

**Purpose:** Check build configuration

**Returns:** Boolean

**Supported configurations:**

- `debug` - Debug build
- `release` - Release build

**Example:**

```css
profile debug {
  optimize: none;
  debug-info: full;
  sanitizers: address, undefined;
}

profile release {
  optimize: aggressive;
  lto: full;
  strip: true;
}
```

---

### Option Checking

**Function:** `option(identifier)`

**Purpose:** Check if a build option is enabled

**Returns:** Boolean

**Example:**

```css
options {
  build_tests: true;
  enable_logging: false;
}

@if option(BUILD_TESTS) {
  target tests {
    type: test;
    sources: "tests/**/*.cpp";
  }
}

@if option(ENABLE_LOGGING) {
  target myapp {
    defines: ENABLE_LOGGING;
    depends: spdlog;
  }
}
```

---

### Feature Checking

**Function:** `feature(identifier)` and `has-feature(identifier)`

**Purpose:**

- `feature()` - Check if optional dependency feature is enabled
- `has-feature()` - Check if compiler/CPU feature is available

**Returns:** Boolean

**Example:**

```css
dependencies {
    vulkan?: "1.3";  // Optional dependency
}

@if feature(vulkan) {
    target renderer {
        sources: "src/vulkan/**/*.cpp";
        depends: vulkan;
        defines: RENDERER_VULKAN;
    }
}

@if has-feature(avx2) {
    target myapp {
        compile-options: -mavx2, -mfma;
        defines: HAS_AVX2;
    }
}
```

---

### File System

**Function:** `exists(path)`

**Purpose:** Check if file or directory exists

**Returns:** Boolean

**Example:**

```css
@if exists("vendor/custom-lib") {
    target myapp {
        include-dirs: "vendor/custom-lib/include";
        link-dirs: "vendor/custom-lib/lib";
    }
}

@if exists("config.json") {
    // Use custom configuration
}
```

---

### File Globbing

**Function:** `glob(pattern)`

**Purpose:** Match files using glob patterns

**Returns:** List of file paths

**Patterns:**

- `*` - Match any characters except `/`
- `**` - Match any characters including `/` (recursive)
- `?` - Match single character
- `[abc]` - Match any character in set
- `{a,b}` - Match any of the alternatives

**Example:**

```css
@for file in glob("plugins/*.cpp") {
    target plugin-${file.stem} {
        type: shared-lib;
        sources: ${file};
    }
}
```

---

### Git Dependencies

**Function:** `git(url)`

**Purpose:** Specify a Git repository dependency

**Returns:** Dependency value

**Options:**

- `tag: String` - Git tag

**Example:**

```css
dependencies {
  imgui: git("https://github.com/ocornut/imgui")
  {
    tag: "v1.90";
  }
}
```

---

## Expressions and Values

### Value Types

```ebnf
Value = String | Number | Boolean | Identifier | List | FunctionCall ;
```

**String:**

```css
"hello world"
"path/to/file"
"${project.version}"  // With interpolation
```

**Number:**

Note that floating-point numbers and negatives are not supported.

```css
123
0
42
```

**Boolean:**

```css
true
false
```

**Identifier:**

```css
myapp
executable
fmt
```

**List:**

```css
[item1, item2, item3]
item1, item2, item3  // Implicit list (in property values)
```

---

### Operators

**Comparison operators:**

- `==` - Equal
- `!=` - Not equal
- `<` - Less than
- `<=` - Less than or equal
- `>` - Greater than
- `>=` - Greater than or equal

**Logical operators:**

- `and` - Logical AND
- `or` - Logical OR
- `not` - Logical NOT

**Other operators:**

- `..` - Range (e.g., `0..10`)
- `?` - Optional dependency marker
- `$` - String interpolation prefix

**Example:**

```css
@if platform(windows) and arch(x86_64) {
    // 64-bit Windows
}

@if not platform(windows) {
    // Non-Windows
}

@if option(BUILD_TESTS) and (config(debug) or option(FORCE_TESTS)) {
    // Complex condition
}
```

---

## Complete Grammar Reference

### EBNF Notation

```
| = alternation
[ ] = optional
{ } = repetition (zero or more)
( ) = grouping
"..." = terminal (literal)
```

### Full Grammar

```ebnf
(* ===== Entry Point ===== *)
KumiFile = { Statement } ;

(* ===== Statements ===== *)
Statement          = ProjectDecl | WorkspaceDecl | DependenciesDecl | TargetDecl
                     | OptionsDecl | MixinDecl | ProfileDecl | InstallDecl
                     | PackageDecl | ScriptsDecl | ImportDecl
                     | ConditionalBlock | ForLoop | LoopControl | DiagnosticStmt ;

(* ===== Declarations ===== *)
ProjectDecl        = "project" Identifier "{" { PropertyAssignment } "}" ;
WorkspaceDecl      = "workspace" "{" { PropertyAssignment } "}" ;
DependenciesDecl   = "dependencies" "{" { DependencySpec } "}" ;
TargetDecl         = "target" Identifier [ "with" MixinList ] "{" { TargetContent } "}" ;
OptionsDecl        = "options" "{" { OptionSpec } "}" ;
MixinDecl          = "mixin" Identifier "{" { PropertyAssignment | VisibilityBlock } "}" ;
ProfileDecl        = "profile" Identifier [ "with" MixinList ] "{" { PropertyAssignment } "}" ;
InstallDecl        = "install" "{" { PropertyAssignment } "}" ;
PackageDecl        = "package" "{" { PropertyAssignment } "}" ;
ScriptsDecl        = "scripts" "{" { PropertyAssignment } "}" ;
ImportDecl         = "@import" String ";" ;

(* ===== Mixin Composition ===== *)
MixinList          = Identifier { "," Identifier } ;

(* ===== Target Content ===== *)
TargetContent      = PropertyAssignment | VisibilityBlock | Statement ;
VisibilityBlock    = ( "public" | "private" | "interface" ) "{" { PropertyAssignment } "}" ;

(* ===== Dependencies ===== *)
DependencySpec     = Identifier [ "?" ] ":" DependencyValue [ DependencyOptions ] ";" ;
DependencyValue    = String | Identifier | FunctionCall ;
DependencyOptions  = "{" { PropertyAssignment } "}" ;

(* ===== Options ===== *)
OptionSpec         = Identifier ":" Value [ OptionConstraints ] ";" ;
OptionConstraints  = "{" { PropertyAssignment } "}" ;

(* ===== Control Flow ===== *)
ConditionalBlock   = IfBlock [ ElseBlock ] ;
IfBlock            = "@if" Condition "{" { Statement } "}" ;
ElseBlock          = "@else" ( IfBlock | "{" { Statement } "}" ) ;

ForLoop            = "@for" Identifier "in" Iterable "{" { Statement } "}" ;
Iterable           = List | Range | FunctionCall ;
Range              = Number ".." Number ;

LoopControl        = ( "@break" | "@continue" ) ";" ;

(* ===== Diagnostics ===== *)
DiagnosticStmt     = ( "@error" | "@warning" | "@info" | "@debug" ) String ";" ;

(* ===== Conditions ===== *)
Condition          = LogicalExpr ;
LogicalExpr        = ComparisonExpr { ( "and" | "or" ) ComparisonExpr } ;
ComparisonExpr     = UnaryExpr [ ComparisonOp UnaryExpr ] ;
ComparisonOp       = "==" | "!=" | "<" | ">" | "<=" | ">=" ;
UnaryExpr          = [ "not" ] PrimaryExpr ;
PrimaryExpr        = FunctionCall | Identifier | Boolean | "(" LogicalExpr ")" ;

(* ===== Properties ===== *)
PropertyAssignment = Identifier ":" ValueList ";" ;
ValueList          = Value { "," Value } ;

(* ===== Values ===== *)
Value              = String | Number | Boolean | Identifier | FunctionCall | Interpolation ;
List               = "[" Value { "," Value } "]" ;
Interpolation      = String ;  (* Contains ${...} *)

(* ===== Functions ===== *)
FunctionCall = Identifier "(" [ ArgumentList ] ")" [ FunctionOptions ] ;
ArgumentList = Value { "," Value } ;
FunctionOptions = "{" { PropertyAssignment } "}" ;
```
