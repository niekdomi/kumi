# Kumi Goals

## Essential Commands

| Command                | Effect                        | Cargo Equivalent        | Notes                     |
| ---------------------- | ----------------------------- | ----------------------- | ------------------------- |
| `kumi init`            | Create new project            | `cargo init`            | Interactive or with flags |
| `kumi build`           | Build project                 | `cargo build`           | Debug by default          |
| `kumi build --release` | Build optimized               | `cargo build --release` | Release mode              |
| `kumi run`             | Build + run executable        | `cargo run`             | Convenience               |
| `kumi test`            | Run tests                     | `cargo test`            | Runs all tests            |
| `kumi clean`           | Remove build artifacts        | `cargo clean`           | Clean build directory     |
| `kumi check`           | Check syntax without building | `cargo check`           | Fast validation           |

---

## Package Management

| Command              | Effect              | Cargo Equivalent | Notes                        |
| -------------------- | ------------------- | ---------------- | ---------------------------- |
| `kumi add <pkg>`     | Add dependency      | `cargo add`      | Updates build.kumi           |
| `kumi remove <pkg>`  | Remove dependency   | `cargo remove`   | Updates build.kumi           |
| `kumi update`        | Update dependencies | `cargo update`   | Respects version constraints |
| `kumi search <pkg>`  | Search packages     | `cargo search`   | Search registry              |
| `kumi install <pkg>` | Install binary      | `cargo install`  | Global install               |

---

## Development

| Command      | Effect               | Go Equivalent | Notes                 |
| ------------ | -------------------- | ------------- | --------------------- |
| `kumi fmt`   | Format build.kumi    | `go fmt`      | Consistent formatting |
| `kumi lint`  | Lint build.kumi      | -             | Check for issues      |
| `kumi doc`   | Generate docs        | `cargo doc`   | From comments         |
| `kumi tree`  | Show dependency tree | `cargo tree`  | Visualize deps        |
| `kumi bench` | Run benchmarks       | `cargo bench` | Performance tests     |

---

## Detailed Command Specs

### 1. `kumi init` (Most Important!)

**Problem it solves:**

Current pain (CMake):

```bash
mkdir myproject && cd myproject
touch CMakeLists.txt
# Now manually write:
# cmake_minimum_required(VERSION 3.20)
# project(myproject)
# add_executable(myproject main.cpp)
# ... 20 more lines

touch main.cpp
# Write boilerplate...
# 30 minutes later, finally ready to code
```

**Kumi solution:**

```bash
kumi init
# Interactive prompts:
# Project name: myproject
# Type: [executable/library/header-only] executable
# C++ standard: [17/20/23] 23
# License: [MIT/Apache-2.0/GPL-3.0] MIT
# Git: [yes/no] yes

# Creates:
# build.kumi
# src/main.cpp
# .gitignore
# README.md
# LICENSE
```

**Generated `build.kumi`:**

```css
project myproject {
  version: "0.1.0";
  license: "MIT";
}

target myproject {
  type: executable;
  sources: "src/**/*.cpp";

  public {
    cpp-standard: "23";
  }
}
```

### 3. `kumi add <package>` (Killer Feature!)

**Problem it solves:**

```bash
# CMake - painful
# 1. Find package
# 2. Add find_package() or FetchContent
# 3. Add target_link_libraries()
# 4. Hope it works

# Meson - better but manual
# Edit meson.build
# Add dependency('fmt')
```

**Kumi solution:**

```bash
kumi add fmt
# Automatically:
# 1. Finds latest version
# 2. Updates build.kumi
# 3. Downloads on next build
```

**Updates `build.kumi`:**

```css
dependencies {
  fmt: "10.2.1"; /* Auto-added */
}

target myapp {
  dependencies: fmt; /* Auto-added */
}
```

**Variants:**

```bash
kumi add fmt@10.0.0           # Specific version
kumi add fmt --optional       # Optional dependency
kumi add fmt --dev            # Dev dependency (tests only)
kumi add my-lib --git https://github.com/user/lib
```

---

### 4. `kumi build` (Simple!)

```bash
# Debug build (default)
kumi build

# Release build
kumi build --release

# Specific target
kumi build myapp

# Parallel build (auto-detected)
kumi build -j 8

# Verbose
kumi build --verbose
```

**vs CMake:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 8 --verbose
```

---

### 5. `kumi run` (Convenience!)

```bash
# Build + run in one command
kumi run

# Specific target
kumi run myapp

# Release mode
kumi run --release
```

---

### 6. `kumi test`

```bash
# Run all tests
kumi test

# Run specific test
kumi test test_utils

# With filter
kumi test --filter "math*"

# Show output
kumi test --verbose
```

**Auto-discovers tests:**

```css
testing {
  framework: catch2;

  test test_utils {
    sources: "tests/test_utils.cpp";
  }
}
```

---

### 7. `kumi check` (Fast Validation)

```bash
# Check syntax without building
kumi check

# Faster than full build
# Useful for CI or pre-commit hooks
```

---

### 8. `Parser + Lexer`

The parser + lexer is about 2.5x faster than the one used by CMake, though the
real game changer is the syntax. It is not only much more readable, but the fact
that it's also more concise results in 8.3x smaller file size. Calculating this
to the 2.5x faster parsing speed, results in a ~20x faster time for reading and
understanding build files than CMake (the same number was also measured).

---

## Comparison with Other Build Systems

| Feature             | CMake                  | Bazel                  | Meson           | xmake           | **Kumi**                      |
| ------------------- | ---------------------- | ---------------------- | --------------- | --------------- | ----------------------------- |
| **Syntax**          | ❌ Imperative, verbose | ⚠️ Verbose Python-like | ⚠️ Python-like  | ⚠️ Lua-based    | ✅ **Declarative CSS-like**   |
| **Package Manager** | ❌ No (manual)         | ❌ No                  | ❌ No           | ⚠️ Limited      | ✅ **Built-in**               |
| **Quick Start**     | ❌ 30 min setup        | ❌ 60 min setup        | ⚠️ 15 min setup | ⚠️ 10 min setup | ✅ **30 sec (`kumi init`)**   |
| **Commands**        | ❌ Confusing flags     | ⚠️ OK                  | ⚠️ OK           | ✅ Good         | ✅ **Intuitive (Cargo-like)** |
| **Error Messages**  | ❌ Poor                | ⚠️ OK                  | ✅ Good         | ⚠️ OK           | ✅ **Excellent (Rust-level)** |
| **Speed (small)**   | ⚠️ OK                  | ❌ Slow (JVM)          | ⚠️ OK (Python)  | ✅ Fast         | ✅ **Very Fast (native)**     |
| **Speed (huge)**    | ❌ Slow reconfigure    | ✅ Fast                | ⚠️ OK           | ⚠️ OK           | ✅ **Fast (incremental)**     |
| **Incremental**     | ❌ Full reconfigure    | ✅ Excellent           | ⚠️ OK           | ⚠️ OK           | ✅ **Excellent**              |
| **Type Safety**     | ❌ None (strings)      | ⚠️ Some                | ⚠️ Some         | ⚠️ Some         | ✅ **Strong**                 |
| **Learning Curve**  | ❌ Steep (weeks)       | ❌ Very steep          | ✅ Easy         | ✅ Easy         | ✅ **Very Easy (30 min)**     |
| **Documentation**   | ❌ Poor                | ⚠️ OK                  | ✅ Good         | ⚠️ OK           | ✅ **Excellent**              |
| **Implementation**  | C++ (legacy)           | Java                   | Python          | C++/Lua         | ✅ **Modern C++23**           |
| **Code Quality**    | ⭐⭐                   | ⭐⭐⭐                 | ⭐⭐⭐⭐        | ⭐⭐⭐          | ✅ **⭐⭐⭐⭐⭐**             |
| **Ecosystem**       | ✅ Huge                | ⚠️ Medium              | ✅ Growing      | ⚠️ Small        | ⚠️ **New**                    |
| **Cross-platform**  | ✅ Yes                 | ✅ Yes                 | ✅ Yes          | ✅ Yes          | ✅ **Yes**                    |

---

# Kumi's Key Selling Points

## Core Value Propositions

### **1. Built-in Package Manager** ⭐⭐⭐⭐⭐

**The killer feature C++ desperately needs**

```bash
kumi add fmt spdlog boost
# Done. That's it. No vcpkg, no Conan, no FetchContent hell.
```

**vs CMake:**

```cmake
find_package(fmt REQUIRED)  # Pray it's installed
# OR
include(FetchContent)
FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 10.2.1
)
FetchContent_MakeAvailable(fmt)
# 20+ lines per dependency
```

**Impact:** Solves C++'s #1 pain point - dependency management

---

### **2. Beautiful Declarative Syntax** ⭐⭐⭐⭐⭐

**CSS-like, self-documenting, type-safe**

```css
target myapp {
  type: executable;
  sources: "src/**/*.cpp";
  depends: fmt, spdlog;

  warnings: strict;
  optimize: speed;
}
```

**vs CMake (same functionality):**

```cmake
add_executable(myapp)
file(GLOB_RECURSE SOURCES "src/*.cpp")
target_sources(myapp PRIVATE ${SOURCES})
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
target_link_libraries(myapp PRIVATE fmt::fmt spdlog::spdlog)
target_compile_options(myapp PRIVATE -Wall -Wextra -Wpedantic -Werror)
set_target_properties(myapp PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(myapp PRIVATE -O3)
endif()
```

**Impact:** 5-10x less code, infinitely more readable

---

### **3. Instant Project Setup** ⭐⭐⭐⭐⭐

**30 seconds vs 30 minutes**

```bash
kumi init myproject
cd myproject
kumi build
kumi run
# You're coding. Already.
```

**vs CMake:**

```bash
mkdir myproject && cd myproject
# Now write CMakeLists.txt (20+ lines)
# Set up directory structure
# Configure vcpkg or Conan
# Write .gitignore
# 30 minutes later...
cmake -B build
cmake --build build
./build/myproject
```

**Impact:** Removes all friction from starting C++ projects

---

### **4. Cargo-like Commands** ⭐⭐⭐⭐⭐

**Intuitive, memorable, consistent**

```bash
kumi build              # Just works
kumi run myapp          # Build + run
kumi test               # Run all tests
kumi add fmt            # Add dependency
kumi check              # Fast validation
```

**vs CMake:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 8
ctest --test-dir build --output-on-failure
# Edit CMakeLists.txt manually for dependencies
# No fast validation - must run full configure
```

**Impact:** Developer experience on par with Rust/Go

---

### **5. Rust-Level Error Messages** ⭐⭐⭐⭐⭐

**Clear, actionable, beautiful**

```sh
error: unknown property 'typ' in target block
  --> build.kumi:5:3
   |
 5 |   typ: executable;
   |   ^^^ help: did you mean 'type'?
   |
```

**vs CMake:**

```sh
CMake Error at CMakeLists.txt:12 (typ):
  Unknown CMake command "typ".
```

**Visibility checking example:**

```sh
error: dependency visibility mismatch
  --> build.kumi:8:5
   |
 8 |     private {
   |     ^^^^^^^ yaml-cpp is marked private
 9 |         depends: yaml-cpp;
   |
note: but yaml-cpp is used in public header
  --> src/config_reader.hpp:11
   |
11 | #include <yaml-cpp/node/convert.h>
   |          ^~~~~~~~~~~~~~~~~~~~~~~~~
   |
help: change to public visibility or move to private implementation
```

**vs CMake:**

```sh
FAILED: [code=1] tests/CMakeFiles/tests.dir/test.cpp.o
/usr/bin/c++ -I/home/user/project/src <... 300 character line ...>
undefined reference to `YAML::Node::convert<std::string>()'
```

**Impact:** Fix issues in seconds instead of hours

---

### **6. Incremental Everything** ⭐⭐⭐⭐⭐

**Sub-second reconfiguration**

```bash
# Edit build.kumi
kumi build
# Reanalyzes in 10-50ms
# Only rebuilds affected files
```

**vs CMake:**

```bash
# Edit CMakeLists.txt
cmake --build build
# Full reconfigure: 5-60 seconds (worse on large projects)
# Sometimes: "Run cmake .. first"
```

**Impact:** 100-1000x faster iteration cycle

---

### **7. Smart Dependency Analysis** ⭐⭐⭐⭐⭐

**Kumi understands your code structure**

```css
global {
  strict-visibility: true; /* Catch visibility errors early */
}
```

**Detects:**

- ✅ Private deps used in public headers (error)
- ✅ Public deps only used privately (warning)
- ✅ Unused dependencies (warning)
- ✅ Circular dependencies (error)

**CMake:** You're on your own. Good luck debugging linker errors.

**Impact:** Cleaner APIs, faster builds, fewer bugs

---

### **8. Cross-Compilation Made Easy** ⭐⭐⭐⭐⭐

**One command, any platform**

```bash
kumi build --target windows-x64
kumi build --target linux-arm64
kumi build --target wasm
```

**vs CMake:**

```bash
# Create toolchain-arm64.cmake (50+ lines)
# Set CMAKE_TOOLCHAIN_FILE
# Configure sysroot paths
# Debug for 2 hours why it doesn't work
cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain-arm64.cmake
```

**Impact:** Cross-compilation goes from "expert mode" to trivial

---

### **9. Binary Caching** ⭐⭐⭐⭐⭐

**Never rebuild the same code twice**

```bash
kumi build --cache-remote https://cache.company.com
# Pulls prebuilt artifacts for unchanged dependencies
# 100x faster CI/CD builds
```

**vs CMake:** No built-in solution. Roll your own with ccache/sccache.

**Impact:** CI/CD builds go from 20 minutes to 2 minutes

---

### **10. Modern C++23 Codebase** ⭐⭐⭐⭐

**Clean, maintainable, extensible**

```cpp
// Kumi codebase uses:
std::expected<Token, Error>    // No exceptions
std::string_view               // Zero-copy
std::span<const Token>         // Safe arrays
Designated initializers        // Clear construction
```

**vs CMake:** Written in 2000s-era C++ with raw pointers and macros

**Impact:** Easy to contribute, sustainable long-term

---

### **11. Fast Validation (`kumi check`)** ⭐⭐⭐⭐⭐

**Instant syntax checking**

```bash
kumi check  # <1ms
# Perfect for:
# - Pre-commit hooks
# - CI pull request checks
# - Editor integration
```

**vs CMake:**

```bash
cmake -B build  # 5-60 seconds just to validate syntax
```

**Impact:** Instant feedback = faster development

---

### **12. LSP & IDE Integration** ⭐⭐⭐⭐

**First-class editor support**

- ✅ Autocomplete for all properties
- ✅ Real-time error checking
- ✅ Go-to-definition (targets, mixins)
- ✅ Hover documentation
- ✅ Rename refactoring

**vs CMake:** Basic syntax highlighting, maybe. LSP is an afterthought.

**Impact:** Professional IDE experience like TypeScript/Rust

---

### **13. Convention Over Configuration** ⭐⭐⭐⭐⭐

**Sensible defaults, minimal boilerplate**

```css
/* This is ALL you need for a simple project */
project myapp {
  version: "1.0.0";
}

/* Auto-discovers:
 * - Sources in src/
 * - Headers in include/
 * - Tests in tests/
 * - Creates executable with project name
 */
```

**vs CMake:** 20+ lines minimum, no conventions.

**Impact:** Less code = less bugs = faster development

---

### **14. Reproducible Builds** ⭐⭐⭐⭐⭐

**Lock file ensures consistency**

```bash
kumi build
# Generates kumi.lock with exact versions
# Commit to git
# Everyone gets identical dependencies
```

**vs CMake:**

```cmake
find_package(fmt)  # Gets whatever version is installed
# "Works on my machine" syndrome
```

**Impact:** No more "works on my machine" - ever

---

### **15. First-Class Testing** ⭐⭐⭐⭐

**Testing is built-in, not bolted-on**

```css
testing {
  framework: catch2;
  timeout: 30s;
  parallel: 4;
}

target my-tests {
  type: test;
  sources: "tests/**/*.cpp";
  depends: mylib;
}
```

```bash
kumi test               # Run all tests
kumi test my-tests      # Run specific test
kumi test --watch       # Re-run on file changes
```

**vs CMake:**

```cmake
enable_testing()
add_executable(my-tests ...)
add_test(NAME my-tests COMMAND my-tests)
# CTest is... not great
```

**Impact:** Testing becomes natural, not a chore

---

### **16. Conditional Compilation Done Right** ⭐⭐⭐⭐⭐

**Readable, powerful, type-safe**

```css
@if platform(windows) {
  target myapp {
    defines: PLATFORM_WINDOWS;
    link-libraries: ws2_32;
  }
}
@else-if platform(macos) {
  target myapp {
    frameworks: Cocoa, Metal;
  }
} @else  {
  target myapp {
    link-libraries: pthread;
  }
}
```

**vs CMake:**

```cmake
if(WIN32)
    target_compile_definitions(myapp PRIVATE PLATFORM_WINDOWS)
    target_link_libraries(myapp PRIVATE ws2_32)
elseif(APPLE)
    target_link_libraries(myapp PRIVATE "-framework Cocoa" "-framework Metal")
else()
    target_link_libraries(myapp PRIVATE pthread)
endif()
```

**Impact:** Platform-specific code is clear and maintainable

---

### **17. World-Class Documentation** ⭐⭐⭐⭐⭐

**Zero to productive in 30 minutes**

- ✅ Assumes no prior knowledge
- ✅ Clear examples for every feature
- ✅ Migration guide from CMake
- ✅ Best practices built-in
- ✅ Searchable, fast, modern website

**vs CMake:** Fragmented wiki, outdated tutorials, "just Google it"

**Impact:** Learning curve measured in minutes, not months

---

### **18. No Macro Hell** ⭐⭐⭐⭐⭐

**No functions, no macros, pure declarative**

```css
/* Kumi: No way to create complex abstractions */
/* This is a FEATURE, not a limitation */
/* Build configs should be simple and readable */
```

**vs CMake:**

```cmake
function(my_complicated_function ARG1 ARG2)
    # 50 lines of logic
    # Nested loops
    # String manipulation
    # Good luck understanding this in 6 months
endfunction()
```

**Impact:** Build files stay maintainable forever

---

## Summary Tables

### **For Individual Developers**

| Feature                      | Rating     | Impact                     | vs CMake                           |
| ---------------------------- | ---------- | -------------------------- | ---------------------------------- |
| **Built-in Package Manager** | ⭐⭐⭐⭐⭐ | No more dependency hell    | CMake: ⭐ (find_package nightmare) |
| **Beautiful Syntax**         | ⭐⭐⭐⭐⭐ | 5-10x less code            | CMake: ⭐⭐ (verbose, imperative)  |
| **Instant Setup**            | ⭐⭐⭐⭐⭐ | Start coding in 30 seconds | CMake: ⭐ (30+ min setup)          |
| **Intuitive Commands**       | ⭐⭐⭐⭐⭐ | `kumi build`, `kumi run`   | CMake: ⭐⭐ (cryptic flags)        |
| **Error Messages**           | ⭐⭐⭐⭐⭐ | Fix in seconds, not hours  | CMake: ⭐ (cryptic errors)         |
| **Fast Validation**          | ⭐⭐⭐⭐⭐ | <1ms feedback              | CMake: ⭐ (must reconfigure)       |
| **World-Class Docs**         | ⭐⭐⭐⭐⭐ | Learn in 30 minutes        | CMake: ⭐⭐ (fragmented)           |
| **IDE Integration**          | ⭐⭐⭐⭐   | Autocomplete, errors, nav  | CMake: ⭐⭐ (basic only)           |
| **Convention over Config**   | ⭐⭐⭐⭐⭐ | Minimal boilerplate        | CMake: ⭐ (no conventions)         |
| **No Macro Hell**            | ⭐⭐⭐⭐⭐ | Always readable            | CMake: ⭐ (function/macro abuse)   |

### **For Teams & Enterprises**

| Feature                   | Rating     | Business Value                | vs CMake                              |
| ------------------------- | ---------- | ----------------------------- | ------------------------------------- |
| **Onboarding Time**       | ⭐⭐⭐⭐⭐ | Hours, not weeks              | CMake: ⭐⭐ (weeks to master)         |
| **Dependency Management** | ⭐⭐⭐⭐⭐ | No "works on my machine"      | CMake: ⭐ (version chaos)             |
| **Incremental Builds**    | ⭐⭐⭐⭐⭐ | 100x faster iteration         | CMake: ⭐⭐⭐ (slow reconfigure)      |
| **Reproducible Builds**   | ⭐⭐⭐⭐⭐ | Lock file ensures consistency | CMake: ⭐⭐ (version conflicts)       |
| **Binary Caching**        | ⭐⭐⭐⭐⭐ | 10x faster CI/CD              | CMake: ⭐ (no built-in solution)      |
| **Smart Dep Analysis**    | ⭐⭐⭐⭐⭐ | Catch visibility errors early | CMake: ⭐ (manual debugging)          |
| **Cross-Compilation**     | ⭐⭐⭐⭐⭐ | One command, any platform     | CMake: ⭐⭐ (complex toolchains)      |
| **Clear Error Messages**  | ⭐⭐⭐⭐⭐ | Less debug time = lower costs | CMake: ⭐ (cryptic errors)            |
| **Standardized Workflow** | ⭐⭐⭐⭐⭐ | Same commands everywhere      | CMake: ⭐⭐⭐ (project-specific)      |
| **Easy Maintenance**      | ⭐⭐⭐⭐⭐ | Anyone can edit build files   | CMake: ⭐⭐ (expert knowledge needed) |
| **Testing Integration**   | ⭐⭐⭐⭐   | Built-in, not bolted-on       | CMake: ⭐⭐ (CTest is clunky)         |
| **Migration Path**        | ⭐⭐⭐⭐   | Gradual adoption possible     | N/A                                   |

---

## The Bottom Line

- **Kumi doesn't just match CMake - it fundamentally reimagines what a C++ build system should be.**
- **Kumi is to CMake what Cargo is to Make - a generational leap forward.** 🚀
- **Finally, the build system C++ deserves.**
