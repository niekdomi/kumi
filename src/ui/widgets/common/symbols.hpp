/// @file symbols.hpp
/// @brief Unicode symbols for terminal UI widgets
///
/// Centralizes all UI symbols used across TUI widgets for consistency
/// and easy customization.

#pragma once

#include <array>
#include <string_view>

namespace kumi::ui::symbols {

//===----------------------------------------------------------------------===//
// Status Indicators
//===----------------------------------------------------------------------===//

static constexpr std::string_view SUCCESS = "✓";
static constexpr std::string_view ERROR = "✗";
static constexpr std::string_view WARNING = "⚠";
static constexpr std::string_view INFO = "ℹ";

//===----------------------------------------------------------------------===//
// Progress & Activity
//===----------------------------------------------------------------------===//

static constexpr std::array<std::string_view, 10> SPINNER_DOTS =
  {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};

static constexpr std::string_view PROGRESS_FILLED = "━";
static constexpr std::string_view PROGRESS_EMPTY = "─";

//===----------------------------------------------------------------------===//
// Selection Indicators
//===----------------------------------------------------------------------===//

static constexpr std::string_view RADIO_SELECTED = "●";
static constexpr std::string_view RADIO_UNSELECTED = "○";

static constexpr std::string_view CHECKBOX_CHECKED = "☑";
static constexpr std::string_view CHECKBOX_UNCHECKED = "☐";

//===----------------------------------------------------------------------===//
// Tree & Hierarchy
//===----------------------------------------------------------------------===//

static constexpr std::string_view TREE_BRANCH = "└─";
static constexpr std::string_view TREE_BRANCH_MID = "├─";
static constexpr std::string_view TREE_PIPE = "│";
static constexpr std::string_view TREE_SPACE = "  ";

//===----------------------------------------------------------------------===//
// Operations
//===----------------------------------------------------------------------===//

static constexpr std::string_view ADDED = "+";
static constexpr std::string_view REMOVED = "-";
static constexpr std::string_view UPDATED = "~";

} // namespace kumi::ui::symbols

