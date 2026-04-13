// Registry of builtin functions, variables, and dependency functions used by
// the semantic checker for validation of conditions, iterables, and dependency
// specs.

//===----------------------------------------------------------------------===//
// Builtin types
//===----------------------------------------------------------------------===//

/// Return type of a builtin function or variable.
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum BuiltinType {
    /// Returns a string value from a known set.
    String,
    /// Returns a boolean.
    Boolean,
    /// Returns a list (for iterables).
    List,
}

/// Where a builtin function may be used.
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum BuiltinContext {
    /// Allowed in conditions (@if).
    Condition,
    /// Allowed as an iterable (@for ... in).
    Iterable,
}

/// A builtin function definition with type info and valid values.
pub struct BuiltinFunction {
    pub name: &'static str,
    pub arg_count: usize,
    pub return_type: BuiltinType,
    pub context: BuiltinContext,
    /// If non-empty, the set of valid string values this function can be
    /// compared against. Empty means any value is accepted.
    pub valid_values: &'static [&'static str],
}

/// A builtin variable (zero-arg, usable as bare identifier in conditions).
pub struct BuiltinVariable {
    pub name: &'static str,
    pub var_type: BuiltinType,
    /// If non-empty, the set of valid string values for comparison.
    pub valid_values: &'static [&'static str],
}

//===----------------------------------------------------------------------===//
// Builtin function registry
//===----------------------------------------------------------------------===//

/// Sorted alphabetically by name for binary search.
pub const BUILTIN_FUNCTIONS: &[BuiltinFunction] = &[
    BuiltinFunction {
        name: "arch",
        arg_count: 0,
        return_type: BuiltinType::String,
        context: BuiltinContext::Condition,
        valid_values: &[
            "aarch64",
            "arm",
            "arm64",
            "mips",
            "mips64",
            "powerpc",
            "powerpc64",
            "riscv32",
            "riscv64",
            "s390x",
            "sparc",
            "sparc64",
            "wasm32",
            "wasm64",
            "x86",
            "x86_64",
        ],
    },
    BuiltinFunction {
        name: "config",
        arg_count: 0,
        return_type: BuiltinType::String,
        context: BuiltinContext::Condition,
        valid_values: &["debug", "minsizerel", "release", "relwithdebinfo"],
    },
    BuiltinFunction {
        name: "files",
        arg_count: 1,
        return_type: BuiltinType::List,
        context: BuiltinContext::Iterable,
        valid_values: &[],
    },
    BuiltinFunction {
        name: "glob",
        arg_count: 1,
        return_type: BuiltinType::List,
        context: BuiltinContext::Iterable,
        valid_values: &[],
    },
    BuiltinFunction {
        name: "has_dep",
        arg_count: 1,
        return_type: BuiltinType::Boolean,
        context: BuiltinContext::Condition,
        valid_values: &[],
    },
    BuiltinFunction {
        name: "has_feature",
        arg_count: 1,
        return_type: BuiltinType::Boolean,
        context: BuiltinContext::Condition,
        valid_values: &[
            "avx", "avx2", "avx512", "neon", "sse", "sse2", "sse3", "sse4_1", "sse4_2", "ssse3",
        ],
    },
    BuiltinFunction {
        name: "option",
        arg_count: 1,
        return_type: BuiltinType::Boolean,
        context: BuiltinContext::Condition,
        valid_values: &[],
    },
    BuiltinFunction {
        name: "platform",
        arg_count: 0,
        return_type: BuiltinType::String,
        context: BuiltinContext::Condition,
        valid_values: &[
            "android",
            "emscripten",
            "freebsd",
            "fuchsia",
            "ios",
            "linux",
            "macos",
            "netbsd",
            "openbsd",
            "wasi",
            "windows",
        ],
    },
];

//===----------------------------------------------------------------------===//
// Builtin variable registry
//===----------------------------------------------------------------------===//

/// Builtin variables usable as bare identifiers in conditions.
/// Sorted alphabetically by name for binary search.
pub const BUILTIN_VARIABLES: &[BuiltinVariable] = &[
    BuiltinVariable {
        name: "arch",
        var_type: BuiltinType::String,
        valid_values: &[
            "aarch64",
            "arm",
            "arm64",
            "mips",
            "mips64",
            "powerpc",
            "powerpc64",
            "riscv32",
            "riscv64",
            "s390x",
            "sparc",
            "sparc64",
            "wasm32",
            "wasm64",
            "x86",
            "x86_64",
        ],
    },
    BuiltinVariable {
        name: "config",
        var_type: BuiltinType::String,
        valid_values: &["debug", "minSizeRelease", "release", "releaseWithDebugInfo"],
    },
    BuiltinVariable {
        name: "debug",
        var_type: BuiltinType::Boolean,
        valid_values: &[],
    },
    BuiltinVariable {
        name: "platform",
        var_type: BuiltinType::String,
        valid_values: &[
            "android",
            "emscripten",
            "freebsd",
            "fuchsia",
            "ios",
            "linux",
            "macos",
            "netbsd",
            "openbsd",
            "wasi",
            "windows",
        ],
    },
    BuiltinVariable {
        name: "release",
        var_type: BuiltinType::Boolean,
        valid_values: &[],
    },
    BuiltinVariable {
        name: "sanitize",
        var_type: BuiltinType::Boolean,
        valid_values: &[],
    },
];

//===----------------------------------------------------------------------===//
// Dependency function registry
//===----------------------------------------------------------------------===//

/// Known functions allowed in dependency values.
/// Sorted alphabetically for binary search.
pub const DEPENDENCY_FUNCTIONS: &[(&str, usize)] = &[("git", 1), ("path", 1), ("system", 0)];

//===----------------------------------------------------------------------===//
// Lookup helpers
//===----------------------------------------------------------------------===//

#[inline]
pub fn lookup_builtin(name: &str) -> Option<&'static BuiltinFunction> {
    BUILTIN_FUNCTIONS
        .binary_search_by_key(&name, |f| f.name)
        .ok()
        .map(|i| &BUILTIN_FUNCTIONS[i])
}

#[inline]
pub fn lookup_builtin_variable(name: &str) -> Option<&'static BuiltinVariable> {
    BUILTIN_VARIABLES
        .binary_search_by_key(&name, |v| v.name)
        .ok()
        .map(|i| &BUILTIN_VARIABLES[i])
}

#[inline]
pub fn is_builtin_variable(name: &str) -> bool {
    lookup_builtin_variable(name).is_some()
}

#[inline]
pub fn lookup_dep_function(name: &str) -> Option<usize> {
    DEPENDENCY_FUNCTIONS
        .binary_search_by_key(&name, |&(n, _)| n)
        .ok()
        .map(|i| DEPENDENCY_FUNCTIONS[i].1)
}

//===----------------------------------------------------------------------===//
// Formatting helpers
//===----------------------------------------------------------------------===//

pub fn builtin_names_list() -> String {
    BUILTIN_FUNCTIONS.iter().map(|f| f.name).collect::<Vec<_>>().join(", ")
}

pub fn dep_function_names_list() -> String {
    DEPENDENCY_FUNCTIONS.iter().map(|(n, _)| *n).collect::<Vec<_>>().join(", ")
}

/// Format a list of valid values for use in error messages.
pub fn format_valid_values(values: &[&str]) -> String {
    values.iter().map(|v| format!("\"{v}\"")).collect::<Vec<_>>().join(", ")
}
