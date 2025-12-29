#pragma once

#include "cli/colors.hpp"
#include "cli/tui/core/ansi.hpp"

#include <format>
#include <string>

namespace kumi {

namespace progress_chars {

constexpr std::string_view FILLED = "━";
constexpr std::string_view EMPTY = "─";

} // namespace progress_chars

class ProgressBar
{
  public:
    explicit ProgressBar(int width = 40) : width_(width) {}

    [[nodiscard]]
    auto render(int current, int total, double speed_mbps = 0.0) const -> std::string
    {
        if (total == 0) {
            return "";
        }

        const double progress = static_cast<double>(current) / static_cast<double>(total);
        const int filled = static_cast<int>(progress * static_cast<double>(width_));

        std::string bar;
        bar.reserve(static_cast<size_t>(width_) + 50);

        for (int i = 0; i < width_; ++i) {
            if (i < filled) {
                bar += std::format("{}{}", color::GREEN, progress_chars::FILLED);
            } else {
                bar += std::format("{}{}", color::DIM, progress_chars::EMPTY);
            }
        }

        if (speed_mbps > 0.0) {
            return std::format("{}{} {}/{} {} • {:.1f} MB/s{}",
                               bar,
                               color::RESET,
                               current,
                               total,
                               color::DIM,
                               speed_mbps,
                               color::RESET);
        }

        return std::format("{}{} {}/{}", bar, color::RESET, current, total);
    }

  private:
    int width_;
};

} // namespace kumi
