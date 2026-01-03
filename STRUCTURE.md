# Kumi Project Structure

## Source Organization

### UI Library (`src/ui/`)
Generic, reusable terminal UI components - library-ready for extraction.

```
ui/
├── core/                          # Core terminal functionality
│   ├── ansi.hpp                   # ANSI escape sequences
│   ├── input.hpp                  # Keyboard input handling
│   ├── raw_mode.hpp               # Raw terminal mode
│   ├── terminal_size.hpp          # Terminal dimensions
│   └── terminal_utils.hpp         # Terminal utilities
└── widgets/                       # Interactive UI widgets
    ├── common/                    # Shared widget utilities
    │   ├── animation.hpp          # Animation controllers
    │   ├── symbols.hpp            # Unicode symbols
    │   └── terminal_state.hpp     # Terminal capabilities
    ├── text_input.hpp             # Text input field
    ├── select.hpp                 # Single-selection menu
    └── multi_select.hpp           # Multi-selection menu
```

**Namespace:** `kumi::ui`
**Purpose:** Clean, documented, library-quality UI components for interactive CLI prompts.

### CLI Commands (`src/cli/`)
Command implementations that use the UI library.

```
cli/
├── colors.hpp                     # ANSI color codes (shared utility)
├── generator.hpp                  # Project generation
├── template_loader.hpp            # Template loading
└── prompt_session.hpp             # Interactive prompt sessions
```

**Namespace:** `kumi::cli`
**Purpose:** Command handlers for `kumi init`, `kumi new`, etc.

### Package Manager (`src/pkg/`)
Package management with domain-specific UI components.

```
pkg/
└── ui/                            # Package manager UI
    ├── primitives/                # Simple reusable widgets
    │   ├── spinner.hpp            # Loading spinner
    │   └── progress_bar.hpp       # Progress bar
    ├── build_package.hpp          # Build state data structure
    ├── build_status_tracker.hpp   # Multi-package build display
    ├── download_progress.hpp      # Package download progress
    └── package_operation.hpp      # Installation summary
```

**Namespace:** `kumi::pkg` (data), `kumi::pkg::ui` (display)
**Purpose:** Package manager specific UI that uses `ui/` primitives + domain logic.

## Design Principles

1. **`ui/` is library-ready** - Generic, well-documented, no domain knowledge
2. **`pkg/ui/` is application-specific** - Tightly coupled to package management
3. **`cli/` orchestrates** - Commands use both `ui/` and `pkg/ui/` as needed
4. **`cli/colors.hpp` is shared** - Cross-cutting concern used everywhere

## Key Files

- **`ui/widgets/common/symbols.hpp`** - All UI symbols in one place
- **`ui/widgets/common/terminal_state.hpp`** - TTY/color detection utility
- **`ui/widgets/common/animation.hpp`** - Shared animation logic
- **`cli/prompt_session.hpp`** - Builder for interactive prompts

## Example Usage

```cpp
// Interactive CLI prompt (kumi init)
#include "cli/prompt_session.hpp"

kumi::cli::PromptSession session;
session.add_text_input("name", "Project name");
session.add_select("type", "Project type", {"executable", "library"}, 0);
session.run();

// Package manager progress display
#include "pkg/ui/primitives/spinner.hpp"

kumi::pkg::ui::Spinner spinner{"Downloading packages"};
spinner.start();
// ... do work ...
spinner.success("Downloaded 10 packages");
```
