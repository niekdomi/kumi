/// @file download_progress.hpp
/// @brief Package download progress tracker

#pragma once

#include "pkg/ui/primitives/progress_bar.hpp"
#include "ui/core/ansi.hpp"
#include "ui/widgets/common/terminal_state.hpp"

#include <atomic>
#include <chrono>
#include <print>
#include <thread>
#include <unistd.h>

namespace kumi::pkg::ui {

/// @brief Tracks and displays download progress for packages
///
/// Shows a progress bar with download count and optional speed information.
class DownloadProgressTracker final
{
  public:
    explicit DownloadProgressTracker(int total_packages,
                                     int bar_width = 40,
                                     kumi::ui::TerminalState term_state = kumi::ui::TerminalState{})
        : total_(total_packages),
          bar_(bar_width, term_state),
          term_state_(term_state)
    {}

    ~DownloadProgressTracker()
    {
        stop();
    }

    DownloadProgressTracker(const DownloadProgressTracker&) = delete;
    auto operator=(const DownloadProgressTracker&) -> DownloadProgressTracker& = delete;
    DownloadProgressTracker(DownloadProgressTracker&&) = delete;
    auto operator=(DownloadProgressTracker&&) -> DownloadProgressTracker& = delete;

    void start()
    {
        if (is_running_.load()) {
            return;
        }

        is_running_.store(true);

        if (!term_state_.is_tty) {
            std::println("Downloading {} packages...", total_);
            return;
        }

        std::print("{}", ansi::CURSOR_HIDE);
        static_cast<void>(std::fflush(stdout));

        animation_thread_ = std::thread([this] { animate(); });
    }

    void update(int current, double speed_mbps = 0.0)
    {
        current_.store(current);
        speed_mbps_.store(speed_mbps);
    }

    void stop()
    {
        if (!is_running_.load()) {
            return;
        }

        is_running_.store(false);

        if (animation_thread_.joinable()) {
            animation_thread_.join();
        }

        if (term_state_.is_tty) {
            std::print("\r{}", ansi::CLEAR_LINE);
            std::print("{}", ansi::CURSOR_SHOW);
            static_cast<void>(std::fflush(stdout));
        }
    }

  private:
    void animate()
    {
        while (is_running_.load()) {
            const int current = current_.load();
            const double speed = speed_mbps_.load();

            const auto bar_str = bar_.render(current, total_, speed);
            std::print("\r{}", bar_str);
            static_cast<void>(std::fflush(stdout));

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    int total_;
    std::atomic<int> current_{0};
    ProgressBar bar_;
    std::atomic<bool> is_running_{false};
    kumi::ui::TerminalState term_state_;
    std::atomic<double> speed_mbps_{0.0};
    std::thread animation_thread_;
};

} // namespace kumi::pkg::ui
