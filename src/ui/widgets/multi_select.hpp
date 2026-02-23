/// @file multi_select.hpp
/// @brief Multi-selection menu widget for terminal UI
///
/// Provides an interactive menu where users can select multiple options
/// using arrow keys, vim-style navigation (j/k), and spacebar to toggle.
///
/// @see Select for single-selection menus
/// @see TextInput for text input widgets

#pragma once

namespace kumi::ui {

/// @brief Interactive multi-selection menu widget
///
/// Displays a list of options with check-boxes that can be toggled.
/// Supports keyboard navigation and returns all selected items.
///
/// Navigation:
/// - Arrow Up/Down or k/j: Move cursor
/// - Space: Toggle selection
/// - Enter: Confirm selection
/// - Ctrl+C: Exit program
class MultiSelect final
{
  public:

  private:
};

} // namespace kumi::ui
