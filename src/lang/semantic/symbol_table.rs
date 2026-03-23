// Symbol table for semantic analysis.
//
// Collects all named declarations (targets, mixins, profiles, options) from one
// or more ASTs and provides lookup and duplicate detection.

use std::collections::HashMap;

/// What kind of declaration a symbol represents.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SymbolKind {
    Target,
    Mixin,
    Profile,
    Option,
}

/// A registered declaration with enough information for diagnostics.
#[derive(Clone, Copy, Debug)]
pub struct SymbolEntry {
    /// Index into the AST's `all_strings` for the symbol name.
    pub name_idx: u32,
    /// Byte offset of the declaration (for "first defined here" errors).
    pub position: u32,
    /// What kind of symbol this is.
    pub kind: SymbolKind,
    /// Which file this symbol was defined in (index into the file list).
    pub file_idx: u16,
}

/// Flat symbol table backed by HashMaps keyed on the symbol name.
///
/// Separate maps per kind so that a target and a mixin can share a name
/// without conflict (different namespaces).
pub struct SymbolTable<'a> {
    targets: HashMap<&'a str, SymbolEntry>,
    mixins: HashMap<&'a str, SymbolEntry>,
    profiles: HashMap<&'a str, SymbolEntry>,
    options: HashMap<&'a str, SymbolEntry>,
}

/// The result of attempting to register a symbol that already exists.
pub struct DuplicateSymbol {
    pub existing: SymbolEntry,
    pub duplicate: SymbolEntry,
}

impl<'a> SymbolTable<'a> {
    pub fn new() -> Self {
        Self {
            targets: HashMap::new(),
            mixins: HashMap::new(),
            profiles: HashMap::new(),
            options: HashMap::new(),
        }
    }

    /// Attempt to register a symbol. Returns `Err` if a symbol with the same
    /// name and kind already exists.
    pub fn register(&mut self, name: &'a str, entry: SymbolEntry) -> Result<(), DuplicateSymbol> {
        let table = self.table_mut(entry.kind);
        if let Some(&existing) = table.get(name) {
            return Err(DuplicateSymbol {
                existing,
                duplicate: entry,
            });
        }
        table.insert(name, entry);
        Ok(())
    }

    /// Look up a symbol by name and kind.
    pub fn lookup(&self, name: &str, kind: SymbolKind) -> Option<&SymbolEntry> {
        self.table(kind).get(name)
    }

    /// Returns all registered names of a given kind (for "did you mean?" suggestions).
    pub fn names(&self, kind: SymbolKind) -> impl Iterator<Item = &str> {
        self.table(kind).keys().copied()
    }

    fn table(&self, kind: SymbolKind) -> &HashMap<&'a str, SymbolEntry> {
        match kind {
            SymbolKind::Target => &self.targets,
            SymbolKind::Mixin => &self.mixins,
            SymbolKind::Profile => &self.profiles,
            SymbolKind::Option => &self.options,
        }
    }

    fn table_mut(&mut self, kind: SymbolKind) -> &mut HashMap<&'a str, SymbolEntry> {
        match kind {
            SymbolKind::Target => &mut self.targets,
            SymbolKind::Mixin => &mut self.mixins,
            SymbolKind::Profile => &mut self.profiles,
            SymbolKind::Option => &mut self.options,
        }
    }
}
