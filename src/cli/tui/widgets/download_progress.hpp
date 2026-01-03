#pragma once

#include "cli/tui/core/ansi.hpp"
#include "cli/tui/widgets/progress_bar.hpp"

#include <atomic>
#include <chrono>
#include <print>
#include <thread>
#include <unistd.h>

namespace kumi {

class DownloadProgressTracker
{
  public:
    explicit DownloadProgressTracker(int total_packages, int bar_width = 40)
        : total_(total_packages),
          current_(0),
          bar_(bar_width),
          is_running_(false),
          is_tty_(isatty(STDOUT_FILENO) != 0),
          color_enabled_(is_tty_ && (std::getenv("NO_COLOR") == nullptr)),
          start_time_(std::chrono::steady_clock::now())
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
        start_time_ = std::chrono::steady_clock::now();

        if (!is_tty_) {
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

        if (is_tty_) {
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
    std::atomic<int> current_;
    ProgressBar bar_;
    std::atomic<bool> is_running_;
    bool is_tty_;
    bool color_enabled_;
    std::atomic<double> speed_mbps_{0.0};
    std::chrono::steady_clock::time_point start_time_;
    std::thread animation_thread_;
};

} // namespace kumi
