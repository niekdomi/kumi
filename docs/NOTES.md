# Kumi Implementation Summary & Design

## Key Decisions Summary

### 1. File Extension & Syntax

- **File extension:** `.kumi`
- **Mixin application:** `with` keyword
  ```kumi
  target myapp with strict, optimized {
      sources: "src/**/*.cpp";
  }
  ```

### 2. Property Conflict Resolution

**Two strategies based on property type:**

#### Scalar Properties (ERROR on conflict)

- `type`, `optimize`, `cpp`, `c`, `output-name`, `version`, `lto`, `strip`, etc.
- **Rule:** Only one value allowed, error on conflict

#### List Properties (APPEND on conflict)

- `sources`, `depends`, `link-libraries`, `defines`, `compile-options`, `frameworks`
- **Rule:** Merge/append values from all mixins

### 3. Import Resolution

- **Circular imports:** Error with full import chain
- **Symbol visibility:** Imported symbols visible in importing scope
- **Duplicate definitions:** Error on duplicate names (mixins, targets, variables)
- **Glob patterns:** Relative to file location

### 4. Source File Validation

- **Rule:** Each source file compiled by exactly ONE target
- **Detection:** Canonical path comparison after glob resolution
- **Action:** Error with helpful suggestions

---

## Error Message Examples

### Error 1: Circular Import

```
Error: Circular import detected

  ┌─ main.kumi:5:1
  │
5 │ @import "common.kumi";
  │ ^^^^^^^^^^^^^^^^^^^^^^ imported here
  │
  ┌─ common.kumi:3:1
  │
3 │ @import "utils.kumi";
  │ ^^^^^^^^^^^^^^^^^^^^^ which imports here
  │
  ┌─ utils.kumi:2:1
  │
2 │ @import "main.kumi";
  │ ^^^^^^^^^^^^^^^^^^^^ which creates a cycle back to main.kumi
  │
  = Import chain: main.kumi → common.kumi → utils.kumi → main.kumi
  = Break the cycle by removing one of the imports
```

---

### Error 2: Duplicate Mixin Definition

```
Error: Duplicate mixin definition 'strict'

  ┌─ common.kumi:3:7
  │
3 │ mixin strict {
  │       ^^^^^^ first defined here
  │
  ┌─ main.kumi:8:7
  │
8 │ mixin strict {
  │       ^^^^^^ redefined here
  │
  = Consider renaming one of them:
    • strict_v1 and strict_v2
    • basic_strict and advanced_strict
```

---

### Error 3: Duplicate Target Definition

```
Error: Duplicate target definition 'myapp'

  ┌─ main.kumi:10:8
  │
10│ target myapp {
  │        ^^^^^ first defined here
  │
  ┌─ main.kumi:20:8
  │
20│ target myapp {
  │        ^^^^^ redefined here
  │
  = Each target must have a unique name
  = Did you mean to use @if conditions for platform-specific targets?
```

---

### Error 4: Scalar Property Conflict in Mixin

```
Error: Property 'optimize' has conflicting values in mixin composition

  ┌─ mixins.kumi:3:5
  │
3 │     optimize: speed;
  │     ^^^^^^^^^^^^^^^ set to 'speed' in mixin 'fast'
  │
  ┌─ mixins.kumi:8:5
  │
8 │     optimize: size;
  │     ^^^^^^^^^^^^^^ set to 'size' in mixin 'small'
  │
  ┌─ main.kumi:15:12
  │
15│ target myapp with fast, small {
  │                         ^^^^^ conflicting mixin applied here
  │
  = Property 'optimize' can only have one value
  = Solutions:
    • Remove one of the mixins
    • Create a new mixin with the desired optimize value
    • Set 'optimize' explicitly in the target (overrides mixins)
```

---

### Error 5: Duplicate Source File Across Targets

```
Error: Source file compiled by multiple targets

  ┌─ main.kumi:5:14
  │
5 │     sources: "**/*.cpp";
  │              ^^^^^^^^^^ matched 'src/utils.cpp' in target 'myapp'
  │
  ┌─ lib/build.kumi:3:14
  │
3 │     sources: "../**/*.cpp";
  │              ^^^^^^^^^^^^^ also matched 'src/utils.cpp' in target 'mylib'
  │
  = File: /home/user/project/src/utils.cpp
  = Each source file can only belong to one target
  = Solutions:
    • Use more specific glob patterns (e.g., "src/app/**/*.cpp")
    • Move shared code to a separate library target
    • Adjust directory structure to avoid overlaps

Example fix:
    target common {
        type: static-lib;
        sources: "src/common/**/*.cpp";
    }

    target myapp {
        sources: "src/app/**/*.cpp";
        depends: common;
    }
```

---

### Error 6: Undefined Mixin Reference

```
Error: Undefined mixin 'optimized'

  ┌─ main.kumi:12:19
  │
12│ target myapp with strict, optimized {
  │                           ^^^^^^^^^ mixin not found
  │
  = No mixin named 'optimized' is defined
  = Available mixins:
    • strict
    • warnings-all
    • cpp23-features
  = Did you mean 'optimized-build'?
```

---

### Error 7: Undefined Target in Dependencies

```
Error: Undefined target 'json-parser'

  ┌─ main.kumi:15:14
  │
15│     depends: json-parser, http-client;
  │              ^^^^^^^^^^^ target not found
  │
  = No target named 'json-parser' is defined in this project
  = Available targets:
    • myapp
    • mylib
    • utils
  = Did you mean to add it as an external dependency in 'dependencies' block?
```

---

### Warning 1: List Property Extended (Not Error)

```
Info: Property 'sources' extended from multiple mixins

  ┌─ main.kumi:15:12
  │
15│ target myapp with base-sources, extra-sources {
  │              ^^^^ contributes: "src/core/**/*.cpp"
  │                   ^^^^^^^^^^^^^ contributes: "src/extra/**/*.cpp"
  │
  = Final sources list:
    • src/core/**/*.cpp (from 'base-sources')
    • src/extra/**/*.cpp (from 'extra-sources')
  = This is expected behavior for list properties
```

---

## High-Level Class Implementation

### 1. BuildSystem (Root Coordinator)

```cpp
// build/build_system.hpp
#pragma once
#include <filesystem>
#include <memory>
#include "../ast/ast.hpp"
#include "import_resolver.hpp"
#include "../semantic/checker.hpp"
#include "graph.hpp"
#include "executor.hpp"

namespace kumi {

class BuildSystem {
public:
    BuildSystem();

    // Main entry points
    void init_project(const std::filesystem::path& directory,
                     const std::string& template_name);
    void build(const std::filesystem::path& root_file,
              const BuildConfig& config);
    void clean();
    void test();

private:
    // Components
    std::unique_ptr<ImportResolver> import_resolver_;
    std::unique_ptr<SemanticChecker> semantic_checker_;
    std::unique_ptr<BuildGraph> build_graph_;
    std::unique_ptr<BuildExecutor> executor_;

    // State
    std::filesystem::path project_root_;
    AST project_ast_;

    // Internal methods
    void load_project(const std::filesystem::path& root_file);
    void validate_project();
    void construct_build_graph();
    void execute_build(const BuildConfig& config);
};

} // namespace kumi
```

---

### 2. ImportResolver

```cpp
// parse/import_resolver.hpp
#pragma once
#include <filesystem>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "../ast/ast.hpp"
#include "../lex/lexer.hpp"
#include "parser.hpp"

namespace kumi {

struct ImportChain {
    std::vector<std::filesystem::path> chain;

    std::string format() const;
};

class ImportResolver {
public:
    ImportResolver();

    // Main resolution
    AST resolve(const std::filesystem::path& root_file);

    // Query
    bool was_imported(const std::filesystem::path& file) const;
    const std::vector<std::filesystem::path>& get_all_files() const;

private:
    // State
    std::unordered_set<std::filesystem::path> import_stack_;
    std::unordered_map<std::filesystem::path, AST> cache_;
    std::vector<std::filesystem::path> all_imported_files_;

    // Internal resolution
    AST resolve_recursive(const std::filesystem::path& file);
    std::filesystem::path resolve_import_path(
        const std::string& import_path,
        const std::filesystem::path& importing_from);

    // Circular detection
    void check_circular(const std::filesystem::path& file);
    ImportChain build_import_chain(const std::filesystem::path& file) const;

    // Parsing
    Lexer lexer_;
    Parser parser_;
};

} // namespace kumi
```

---

### 3. SemanticChecker

```cpp
// semantic/checker.hpp
#pragma once
#include "../ast/ast.hpp"
#include "symbol_table.hpp"
#include "scope.hpp"
#include <vector>
#include <memory>

namespace kumi {

enum class PropertyMergeStrategy {
    Error,   // Scalar properties - must not conflict
    Append   // List properties - merge values
};

struct PropertyInfo {
    std::string name;
    PropertyMergeStrategy strategy;

    static PropertyInfo get(const std::string& name);
};

class SemanticChecker {
public:
    SemanticChecker();

    // Main validation
    void check(const AST& ast);

    // Individual checks
    void check_duplicate_definitions(const AST& ast);
    void check_undefined_references(const AST& ast);
    void check_mixin_composition(const AST& ast);
    void check_duplicate_sources(const AST& ast);
    void check_circular_target_dependencies(const AST& ast);
    void check_type_correctness(const AST& ast);

private:
    SymbolTable symbol_table_;
    std::vector<Diagnostic> diagnostics_;

    // Property validation
    void validate_property_conflicts(
        const std::string& property_name,
        const std::vector<PropertyValue>& values,
        const std::vector<SourceLocation>& locations);

    // Mixin resolution
    std::unordered_map<std::string, PropertyValue>
    resolve_mixin_properties(
        const std::vector<std::string>& mixin_names,
        const AST& ast);

    // Source file validation
    struct SourceFileInfo {
        std::filesystem::path canonical_path;
        std::string target_name;
        SourceLocation location;
    };
    std::unordered_map<std::filesystem::path, std::vector<SourceFileInfo>>
    collect_all_sources(const AST& ast);

    // Error reporting
    void report_error(const Diagnostic& diagnostic);
    void report_warning(const Diagnostic& diagnostic);
};

} // namespace kumi
```

---

### 4. SymbolTable

```cpp
// semantic/symbol_table.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include "../ast/ast.hpp"

namespace kumi {

enum class SymbolKind {
    Target,
    Mixin,
    Preset,
    Variable,
    Feature,
    Option
};

struct Symbol {
    std::string name;
    SymbolKind kind;
    SourceLocation definition_location;
    std::any value;  // AST node (TargetDecl, MixinDecl, etc.)
};

class SymbolTable {
public:
    SymbolTable();

    // Registration
    void register_symbol(const Symbol& symbol);
    void register_target(const TargetDecl& target);
    void register_mixin(const MixinDecl& mixin);
    void register_variable(const VariableDecl& variable);

    // Lookup
    std::optional<Symbol> lookup(const std::string& name, SymbolKind kind) const;
    std::optional<TargetDecl> lookup_target(const std::string& name) const;
    std::optional<MixinDecl> lookup_mixin(const std::string& name) const;
    std::optional<VariableDecl> lookup_variable(const std::string& name) const;

    // Query
    bool has_symbol(const std::string& name, SymbolKind kind) const;
    std::vector<std::string> get_all_names(SymbolKind kind) const;

    // Duplicate detection
    std::optional<SourceLocation> find_duplicate(
        const std::string& name,
        SymbolKind kind) const;

private:
    std::unordered_map<std::string, Symbol> targets_;
    std::unordered_map<std::string, Symbol> mixins_;
    std::unordered_map<std::string, Symbol> presets_;
    std::unordered_map<std::string, Symbol> variables_;
    std::unordered_map<std::string, Symbol> features_;
    std::unordered_map<std::string, Symbol> options_;

    std::unordered_map<std::string, Symbol>&
    get_table(SymbolKind kind);

    const std::unordered_map<std::string, Symbol>&
    get_table(SymbolKind kind) const;
};

} // namespace kumi
```

---

### 5. BuildGraph

```cpp
// build/graph.hpp
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "../ast/ast.hpp"

namespace kumi {

enum class NodeType {
    Target,
    SourceFile,
    ObjectFile,
    Library,
    Executable,
    CustomCommand
};

struct BuildNode {
    std::string id;
    NodeType type;
    std::filesystem::path path;
    std::vector<std::string> dependencies;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::string command;

    // Metadata
    std::any metadata;  // Target-specific data
};

class BuildGraph {
public:
    BuildGraph();

    // Construction
    void add_node(const BuildNode& node);
    void add_edge(const std::string& from, const std::string& to);
    void build_from_ast(const AST& ast);

    // Queries
    std::vector<BuildNode> get_build_order() const;
    std::vector<std::string> get_dependencies(const std::string& node_id) const;
    bool has_cycles() const;
    std::vector<std::string> find_cycle() const;

    // Incremental build support
    bool is_up_to_date(const std::string& node_id) const;
    std::vector<std::string> get_outdated_nodes() const;

private:
    std::unordered_map<std::string, BuildNode> nodes_;
    std::unordered_map<std::string, std::vector<std::string>> adjacency_list_;

    // Graph algorithms
    std::vector<std::string> topological_sort() const;
    bool detect_cycle_dfs(
        const std::string& node,
        std::unordered_set<std::string>& visited,
        std::unordered_set<std::string>& rec_stack,
        std::vector<std::string>& cycle_path) const;

    // Target conversion
    void add_target_to_graph(const TargetDecl& target, const AST& ast);
    void resolve_dependencies(const TargetDecl& target, const AST& ast);

    // File timestamp checking
    bool needs_rebuild(const BuildNode& node) const;
    std::filesystem::file_time_type get_newest_input_time(
        const BuildNode& node) const;
    std::filesystem::file_time_type get_oldest_output_time(
        const BuildNode& node) const;
};

} // namespace kumi
```

---

### 6. BuildExecutor

```cpp
// build/executor.hpp
#pragma once
#include "graph.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace kumi {

struct BuildConfig {
    enum class BuildType { Debug, Release, RelWithDebInfo, MinSizeRel };

    BuildType build_type = BuildType::Debug;
    size_t num_threads = std::thread::hardware_concurrency();
    bool verbose = false;
    bool use_cache = true;
    std::optional<std::string> toolchain;
    std::vector<std::string> features;
};

struct BuildResult {
    bool success = false;
    size_t targets_built = 0;
    size_t targets_up_to_date = 0;
    size_t targets_failed = 0;
    std::chrono::milliseconds duration{0};
    std::vector<std::string> error_messages;
};

class BuildExecutor {
public:
    BuildExecutor(const BuildConfig& config);

    // Execution
    BuildResult execute(const BuildGraph& graph);
    void cancel();

    // Progress monitoring
    using ProgressCallback = std::function<void(size_t current, size_t total)>;
    void set_progress_callback(ProgressCallback callback);

private:
    BuildConfig config_;
    std::atomic<bool> cancelled_{false};
    ProgressCallback progress_callback_;

    // Parallel execution
    struct WorkItem {
        BuildNode node;
        size_t priority;
    };

    std::queue<WorkItem> work_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    void worker_thread();
    bool execute_node(const BuildNode& node);

    // Compilation
    bool compile_source(const BuildNode& node);
    bool link_executable(const BuildNode& node);
    bool link_library(const BuildNode& node);
    bool run_custom_command(const BuildNode& node);

    // Compiler detection
    struct CompilerInfo {
        std::string path;
        std::string version;
        enum class Type { GCC, Clang, MSVC, Intel } type;
    };
    CompilerInfo detect_compiler() const;

    // Command generation
    std::string generate_compile_command(
        const BuildNode& node,
        const CompilerInfo& compiler) const;
    std::string generate_link_command(
        const BuildNode& node,
        const CompilerInfo& compiler) const;

    // Output handling
    void print_progress(size_t current, size_t total);
    void print_command(const std::string& command);
    void print_error(const std::string& error);
};

} // namespace kumi
```

---

### 7. Scope (for future scoping needs)

```cpp
// semantic/scope.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>

namespace kumi {

class Scope {
public:
    Scope(Scope* parent = nullptr);

    // Symbol management
    void define(const std::string& name, std::any value);
    std::optional<std::any> lookup(const std::string& name) const;
    bool exists(const std::string& name) const;

    // Scope hierarchy
    Scope* parent() const { return parent_; }
    void add_child(std::unique_ptr<Scope> child);

private:
    Scope* parent_;
    std::unordered_map<std::string, std::any> symbols_;
    std::vector<std::unique_ptr<Scope>> children_;
};

class ScopeManager {
public:
    ScopeManager();

    void push_scope();
    void pop_scope();
    Scope* current_scope();

private:
    std::unique_ptr<Scope> root_;
    Scope* current_;
};

} // namespace kumi
```

---

## Property Merge Strategy Reference

### Scalar Properties (ERROR on conflict)

```cpp
static const std::unordered_set<std::string> SCALAR_PROPERTIES = {
    "type",           // executable, static-lib, etc.
    "optimize",       // none, debug, speed, size, aggressive
    "cpp",            // 17, 20, 23, 26
    "c",              // 11, 17, 23
    "output-name",    // Custom output name
    "version",        // Library version
    "lto",            // true/false
    "strip",          // true/false
    "debug-info",     // none, line-tables, full
    "position-independent",  // true/false
    "visibility",     // default, hidden
    "no-exceptions",  // true/false
    "no-rtti",        // true/false
    "warnings",       // none, all, strict, pedantic
    "warnings-as-errors",  // true/false
    "timeout",        // Test timeout
    // ... more
};
```

### List Properties (APPEND on conflict)

```cpp
static const std::unordered_set<std::string> LIST_PROPERTIES = {
    "sources",           // Source files
    "headers",           // Header files
    "depends",           // Dependencies
    "link-libraries",    // Link libraries
    "link-dirs",         // Library search paths
    "include-dirs",      // Include directories
    "system-include-dirs",  // System includes
    "defines",           // Preprocessor defines
    "compile-options",   // Compiler flags
    "link-options",      // Linker flags
    "frameworks",        // macOS frameworks
    "sanitizers",        // address, thread, etc.
    // ... more
};
```

---

## Implementation Order

### Phase 1: Foundation (Current)

- [x] Lexer
- [x] Parser
- [x] AST

### Phase 2: Core (Next)

- [ ] ImportResolver
- [ ] SymbolTable
- [ ] SemanticChecker
- [ ] Error reporting infrastructure

### Phase 3: Build System

- [ ] BuildGraph
- [ ] BuildExecutor
- [ ] BuildSystem coordinator

### Phase 4: Features

- [ ] Dependency management
- [ ] Package registry
- [ ] Binary cache

### Phase 5: Tooling

- [ ] Formatter
- [ ] LSP
- [ ] IDE integration

---

## Testing Strategy

### Unit Tests

```cpp
// Test mixin property merging
TEST_CASE("Mixin scalar property conflict") {
    // Setup
    MixinDecl m1 = parse("mixin a { optimize: speed; }");
    MixinDecl m2 = parse("mixin b { optimize: size; }");
    TargetDecl t = parse("target app with a, b {}");

    // Execute
    SemanticChecker checker;
    REQUIRE_THROWS_AS(checker.check_mixin_composition(t), SemanticError);
}

TEST_CASE("Mixin list property append") {
    // Setup
    MixinDecl m1 = parse("mixin a { sources: 'a.cpp'; }");
    MixinDecl m2 = parse("mixin b { sources: 'b.cpp'; }");
    TargetDecl t = parse("target app with a, b {}");

    // Execute
    auto resolved = resolve_properties(t);

    // Verify
    REQUIRE(resolved.sources.size() == 2);
    REQUIRE(contains(resolved.sources, "a.cpp"));
    REQUIRE(contains(resolved.sources, "b.cpp"));
}
```

### Integration Tests

```cpp
TEST_CASE("Import resolution with circular dependency") {
    // Create test files
    write_file("a.kumi", "@import 'b.kumi'; target a {}");
    write_file("b.kumi", "@import 'a.kumi'; target b {}");

    // Execute
    ImportResolver resolver;
    REQUIRE_THROWS_AS(resolver.resolve("a.kumi"), CircularImportError);
}
```

---

## Next Steps

1. **Implement ImportResolver**
   - File resolution
   - Circular detection
   - AST merging

2. **Implement SymbolTable**
   - Symbol registration
   - Duplicate detection
   - Lookup functions

3. **Implement SemanticChecker**
   - Duplicate definitions
   - Undefined references
   - Property conflict detection
   - Source file validation

4. **Implement Error Reporting**
   - Beautiful error messages
   - Source location tracking
   - Helpful suggestions

5. **Write Comprehensive Tests**
   - Unit tests for each component
   - Integration tests for full pipeline
   - Error message validation

🚀 **Ready to build Kumi!**
