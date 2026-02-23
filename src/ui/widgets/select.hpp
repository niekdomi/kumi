/// @file select.hpp
/// @brief Single-selection menu widget for terminal UI
///
/// Provides an interactive menu where users can select one option from a list
/// using arrow keys or vim-style navigation (j/k).
///
/// @see MultiSelect for multi-selection menus
/// @see TextInput for text input widgets

#pragma once

namespace kumi::ui {

/// @brief Interactive single-selection menu widget
///
/// Displays a list of options with radio buttons where only one can be selected.
/// Supports keyboard navigation and returns the selected item.
///
/// Navigation:
/// - Arrow Up/Down or k/j: Move cursor
/// - Enter: Confirm selection
/// - Ctrl+C: Exit program
class Select final
{
  public:

  private:
};

} // namespace kumi::ui
