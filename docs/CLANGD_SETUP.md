# Clangd Setup for Kumi

## How It Works

Kumi uses xmake's `compile_commands.json` generation for clangd support.

### Files Involved

1. **`xmake.lua`** - Contains the build configuration
   - Line 10: `add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})`
   - This automatically generates `compile_commands.json` on every build

2. **`.clangd`** - Clangd configuration
   ```yaml
   CompileFlags:
     CompilationDatabase: .
     Compiler: clang
   ```
   - Points to `compile_commands.json` in project root

3. **`compile_commands.json`** - Generated automatically by xmake
   - Contains exact compiler flags for each source file
   - Regenerated on every `xmake` build

### Usage

1. **Build the project** (generates compile_commands.json):
   ```bash
   xmake
   ```

2. **Restart clangd** in your editor:
   - VSCode: Reload window or restart clangd extension
   - Vim/Neovim: `:LspRestart`
   - Other editors: Check documentation

3. **Verify** - clangd should now:
   - Find all headers (`kumi/lex/token.hpp`, etc.)
   - Provide autocomplete
   - Show no "file not found" errors

### Manual Generation

If needed, manually generate compile_commands.json:
```bash
xmake project -k compile_commands
```

### Troubleshooting

**Problem**: clangd still shows errors

**Solutions**:
1. Ensure `compile_commands.json` exists in project root
2. Restart clangd
3. Check `.clangd` points to correct directory
4. Rebuild: `xmake -r`

**Problem**: compile_commands.json not generated

**Solutions**:
1. Run `xmake` at least once
2. Check xmake version: `xmake --version` (needs 2.5.1+)
3. Manually generate: `xmake project -k compile_commands`
