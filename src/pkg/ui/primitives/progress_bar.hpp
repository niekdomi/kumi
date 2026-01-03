/// @file progress_bar.hpp
/// @brief Progress bar widget for terminal UI
///
/// Provides a visual progress bar with percentage and optional speed display.

#pragma once

#include "cli/colors.hpp"
#include "ui/widgets/common/symbols.hpp"
#include "ui/widgets/common/terminal_state.hpp"

#include <cstddef>
#include <format>
#include <string>

namespace kumi::pkg::ui {

/// @brief Visual progress bar widget
///
/// Renders a progress bar showing completion status with optional
/// speed information (e.g., download speed in MB/s).
///
/// Example usage:
/// ```cpp
/// ProgressBar bar{40};
/// auto display = bar.render(50, 100, 2.5);
/// std::println("{}", display);
/// ```
class ProgressBar final
{
  public:
    /// @brief Constructs a progress bar with specified width
    /// @param width Number of characters for the bar portion
    /// @param term_state Terminal capabilities (auto-detected if not provided)
    explicit ProgressBar(int width = 40,
                         kumi::ui::TerminalState term_state = kumi::ui::TerminalState{})
        : width_(width),
          term_state_(term_state)
    {}

    /// @brief Renders the progress bar as a string
    /// @param current Current progress value
    /// @param total Total/maximum progress value
    /// @param speed_mbps Optional speed indicator in MB/s (0 to disable)
    /// @return Formatted progress bar string
    [[nodiscard]]
    auto render(int current, int total, double speed_mbps = 0.0) const -> std::string
    {
        if (total == 0) {
            return "";
        }

        const auto progress = static_cast<double>(current) / static_cast<double>(total);
        const auto filled = static_cast<int>(progress * static_cast<double>(width_));

        std::string bar;
        bar.reserve(static_cast<std::size_t>(width_) + 100);

        // Render filled and empty portions
        for (int i = 0; i < width_; ++i) {
            if (i < filled) {
                bar += std::format(
                  "{}{}", term_state_.color(color::GREEN), kumi::ui::symbols::PROGRESS_FILLED);
            } else {
                bar += std::format(
                  "{}{}", term_state_.color(color::DIM), kumi::ui::symbols::PROGRESS_EMPTY);
            }
        }

        // Add reset and progress numbers
        bar += std::format("{} {}/{}", term_state_.color(color::RESET), current, total);

        // Add optional speed indicator
        if (speed_mbps > 0.0) {
            bar += std::format(" {} • {:.1f} MB/s{}",
                               term_state_.color(color::DIM),
                               speed_mbps,
                               term_state_.color(color::RESET));
        }

        return bar;
    }

    /// @brief Gets the configured width of the progress bar
    /// @return Width in characters
    [[nodiscard]]
    constexpr auto width() const noexcept -> int
    {
        return width_;
    }

  private:
    int width_;
    kumi::ui::TerminalState term_state_;
};

} // namespace kumi::pkg::ui
