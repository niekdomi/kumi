/// @file symbols.hpp
/// @brief Unicode symbols for terminal UI widgets
///
/// Centralizes all UI symbols used across TUI widgets for consistency
/// and easy customization.

#pragma once

#include <string_view>

namespace kumi::ui::symbols {

//===----------------------------------------------------------------------===//
// Status Indicators
//===----------------------------------------------------------------------===//

constexpr std::string_view SUCCESS = "✓";
constexpr std::string_view ERROR = "✗";
constexpr std::string_view WARNING = "⚠";
constexpr std::string_view INFO = "ℹ";

//===----------------------------------------------------------------------===//
// Progress & Activity
//===----------------------------------------------------------------------===//

constexpr std::string_view SPINNER_DOTS[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};

constexpr std::string_view PROGRESS_FILLED = "━";
constexpr std::string_view PROGRESS_EMPTY = "─";

//===----------------------------------------------------------------------===//
// Selection Indicators
//===----------------------------------------------------------------------===//

constexpr std::string_view RADIO_SELECTED = "●";
constexpr std::string_view RADIO_UNSELECTED = "○";

constexpr std::string_view CHECKBOX_CHECKED = "☑";
constexpr std::string_view CHECKBOX_UNCHECKED = "☐";

//===----------------------------------------------------------------------===//
// Tree & Hierarchy
//===----------------------------------------------------------------------===//

constexpr std::string_view TREE_BRANCH = "└─";
constexpr std::string_view TREE_BRANCH_MID = "├─";
constexpr std::string_view TREE_PIPE = "│";
constexpr std::string_view TREE_SPACE = "  ";

//===----------------------------------------------------------------------===//
// Operations
//===----------------------------------------------------------------------===//

constexpr std::string_view ADDED = "+";
constexpr std::string_view REMOVED = "-";
constexpr std::string_view UPDATED = "~";

} // namespace kumi::ui::symbols
