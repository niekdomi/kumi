#pragma once

#include "terminal.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <vector>

namespace kumi {

// Spinner frames (Braille patterns for smooth animation)
namespace spinner_frames {
constexpr std::array<std::string_view, 10> DOTS
  = { "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏" };

constexpr std::array<std::string_view, 8> DOTS_SIMPLE = { "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧" };

constexpr std::array<std::string_view, 4> LINE = { "─", "\\", "│", "/" };
} // namespace spinner_frames

// Status symbols
namespace symbols {
constexpr std::string_view SUCCESS = "✓";
constexpr std::string_view ERROR = "✗";
constexpr std::string_view WARNING = "⚠";
constexpr std::string_view ADDED = "+";
constexpr std::string_view REMOVED = "-";
constexpr std::string_view UPDATED = "~";
constexpr std::string_view TREE_BRANCH = "└─";
constexpr std::string_view TREE_PIPE = "│";
} // namespace symbols

// Progress bar drawing
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

        // Build the bar with green for filled, gray for unfilled
        for (int i = 0; i < width_; ++i) {
            if (i < filled) {
                bar += std::format("{}━", ansi::GREEN);
            } else {
                bar += std::format("{}─", ansi::DIM);
            }
        }

        // Add speed if provided
        if (speed_mbps > 0.0) {
            return std::format("{}{} {}/{} {} • {:.1f} MB/s{}",
                               bar,
                               ansi::RESET,
                               current,
                               total,
                               ansi::DIM,
                               speed_mbps,
                               ansi::RESET);
        }

        return std::format("{}{} {}/{}", bar, ansi::RESET, current, total);
    }

  private:
    int width_;
};

// Spinner with progressive disclosure
class Spinner
{
  public:
    explicit Spinner(std::string_view message = "Working") :
      message_(message),
      is_running_(false),
      is_tty_(isatty(STDOUT_FILENO) != 0),
      color_enabled_(is_tty_ && (std::getenv("NO_COLOR") == nullptr)),
      start_time_(std::chrono::steady_clock::now())
    {
    }

    ~Spinner() { stop(); }

    // Non-copyable, non-movable
    Spinner(const Spinner &) = delete;
    auto operator=(const Spinner &) -> Spinner & = delete;
    Spinner(Spinner &&) = delete;
    auto operator=(Spinner &&) -> Spinner & = delete;

    void start()
    {
        if (is_running_.load()) {
            return;
        }

        is_running_.store(true);
        start_time_ = std::chrono::steady_clock::now();

        if (!is_tty_) {
            // Non-TTY: just print the message once
            std::println("{}...", message_);
            return;
        }

        // Hide cursor and start animation thread
        std::print("{}", ansi::CURSOR_HIDE);
        static_cast<void>(std::fflush(stdout));

        animation_thread_ = std::thread([this] { animate(); });
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
            // Clear the line and show cursor
            std::print("\r{}", ansi::CLEAR_LINE);
            std::print("{}", ansi::CURSOR_SHOW);
            static_cast<void>(std::fflush(stdout));
        }
    }

    void update_message(std::string_view new_message) { message_ = new_message; }

    void success(std::string_view message)
    {
        stop();
        if (color_enabled_) {
            std::println("{}{}{} {}", ansi::GREEN, symbols::SUCCESS, ansi::RESET, message);
        } else {
            std::println("{} {}", symbols::SUCCESS, message);
        }
    }

    // Success with gray text and bold keyword
    void success_with_bold(std::string_view prefix,
                           std::string_view bold_word,
                           std::string_view suffix)
    {
        stop();
        if (color_enabled_) {
            std::println("{}{}{} {}{}{}{}{}{}{}",
                         ansi::GREEN,
                         symbols::SUCCESS,
                         ansi::RESET,
                         ansi::DIM,
                         prefix,
                         ansi::BOLD,
                         bold_word,
                         ansi::RESET,
                         ansi::DIM,
                         suffix,
                         ansi::RESET);
        } else {
            std::println("{} {}{}{}", symbols::SUCCESS, prefix, bold_word, suffix);
        }
    }

    void error(std::string_view message)
    {
        stop();
        if (color_enabled_) {
            std::println("{}{}{} {}", ansi::RED, symbols::ERROR, ansi::RESET, message);
        } else {
            std::println("{} {}", symbols::ERROR, message);
        }
    }

    void warning(std::string_view message)
    {
        stop();
        if (color_enabled_) {
            std::println("{}{}{} {}", ansi::YELLOW, symbols::WARNING, ansi::RESET, message);
        } else {
            std::println("{} {}", symbols::WARNING, message);
        }
    }

    void info(std::string_view message)
    {
        stop();
        std::println(" {} {}", symbols::ADDED, message);
    }

    [[nodiscard]]
    auto elapsed_ms() const -> long long
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - start_time_)
          .count();
    }

    [[nodiscard]]
    auto elapsed_seconds() const -> double
    {
        return static_cast<double>(elapsed_ms()) / 1000.0;
    }

  private:
    void animate()
    {
        while (is_running_.load()) {
            const auto frame = spinner_frames::DOTS[frame_index_];
            const auto elapsed = elapsed_seconds();

            // Progressive disclosure: show timing after 2 seconds
            if (elapsed < 2.0) {
                if (color_enabled_) {
                    std::print("\r{}{}{} {}...", ansi::CYAN, frame, ansi::RESET, message_);
                } else {
                    std::print("\r{} {}...", frame, message_);
                }
            } else {
                if (color_enabled_) {
                    std::print("\r{}{}{} {}... {}{}({:.1f}s){}",
                               ansi::CYAN,
                               frame,
                               ansi::RESET,
                               message_,
                               ansi::DIM,
                               ansi::YELLOW,
                               elapsed,
                               ansi::RESET);
                } else {
                    std::print("\r{} {}... ({:.1f}s)", frame, message_, elapsed);
                }
            }

            static_cast<void>(std::fflush(stdout));

            frame_index_ = (frame_index_ + 1) % spinner_frames::DOTS.size();
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
    }

    std::string message_;
    std::atomic<bool> is_running_;
    bool is_tty_;
    bool color_enabled_;
    size_t frame_index_{ 0 };
    std::chrono::steady_clock::time_point start_time_;
    std::thread animation_thread_;
};

// Download progress tracker with animated progress bar
class DownloadProgressTracker
{
  public:
    explicit DownloadProgressTracker(int total_packages, int bar_width = 40) :
      total_(total_packages),
      current_(0),
      bar_(bar_width),
      is_running_(false),
      is_tty_(isatty(STDOUT_FILENO) != 0),
      color_enabled_(is_tty_ && (std::getenv("NO_COLOR") == nullptr)),
      start_time_(std::chrono::steady_clock::now())
    {
    }

    ~DownloadProgressTracker() { stop(); }

    // Non-copyable, non-movable
    DownloadProgressTracker(const DownloadProgressTracker &) = delete;
    auto operator=(const DownloadProgressTracker &) -> DownloadProgressTracker & = delete;
    DownloadProgressTracker(DownloadProgressTracker &&) = delete;
    auto operator=(DownloadProgressTracker &&) -> DownloadProgressTracker & = delete;

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

        // Hide cursor and start animation thread
        std::print("{}", ansi::CURSOR_HIDE);
        static_cast<void>(std::fflush(stdout));

        animation_thread_ = std::thread([this] { animate(); });
    }

    void update(int current, double speed_mbps = 12.3)
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
            // Clear the line and show cursor
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
    std::atomic<double> speed_mbps_{ 0.0 };
    std::chrono::steady_clock::time_point start_time_;
    std::thread animation_thread_;
};

// Package operation tracker with progressive disclosure
class PackageOperationTracker
{
  public:
    struct Package
    {
        std::string name;
        std::string version;
        bool cached = false;
        bool is_root = false; // true if this is the requested package, false for transitive deps
        size_t size_bytes = 0;
    };

    explicit PackageOperationTracker(std::string_view operation = "Installing") :
      operation_(operation),
      is_tty_(isatty(STDOUT_FILENO) != 0),
      color_enabled_(is_tty_ && (std::getenv("NO_COLOR") == nullptr)),
      start_time_(std::chrono::steady_clock::now())
    {
    }

    void add_package(Package pkg) { packages_.push_back(std::move(pkg)); }

    void show_summary()
    {
        // Find the longest package name for alignment
        size_t max_name_len = 0;
        for (const auto &pkg : packages_) {
            max_name_len = std::max(max_name_len, pkg.name.length());
        }

        // Separate root packages from transitive dependencies
        std::vector<Package> root_packages;
        std::vector<Package> transitive_deps;

        for (const auto &pkg : packages_) {
            if (pkg.is_root) {
                root_packages.push_back(pkg);
            } else {
                transitive_deps.push_back(pkg);
            }
        }

        // Show package list with tree structure
        for (size_t i = 0; i < root_packages.size(); ++i) {
            const auto &pkg = root_packages[i];
            const bool is_last_root = (i == root_packages.size() - 1);

            // Root package
            if (color_enabled_) {
                std::println(" {}{}{} {:<{}} {}v{}{}{}",
                             ansi::GREEN,
                             symbols::ADDED,
                             ansi::RESET,
                             pkg.name,
                             max_name_len,
                             ansi::DIM,
                             pkg.version,
                             ansi::RESET,
                             pkg.cached ? " (cached)" : "");
            } else {
                std::println(" {} {:<{}} v{}{}",
                             symbols::ADDED,
                             pkg.name,
                             max_name_len,
                             pkg.version,
                             pkg.cached ? " (cached)" : "");
            }

            // Show transitive dependencies as tree
            if (!transitive_deps.empty() && is_last_root) {
                for (size_t j = 0; j < transitive_deps.size(); ++j) {
                    const auto &dep = transitive_deps[j];
                    const bool is_last = (j == transitive_deps.size() - 1);
                    const auto tree_char = is_last ? "└─" : "├─";

                    if (color_enabled_) {
                        std::println("   {}{} {:<{}} {}v{}{}{}",
                                     ansi::DIM,
                                     tree_char,
                                     dep.name,
                                     max_name_len,
                                     ansi::DIM,
                                     dep.version,
                                     ansi::RESET,
                                     dep.cached ? " (cached)" : "");
                    } else {
                        std::println("   {} {:<{}} v{}{}",
                                     tree_char,
                                     dep.name,
                                     max_name_len,
                                     dep.version,
                                     dep.cached ? " (cached)" : "");
                    }
                }
            }
        }
    }

    [[nodiscard]]
    auto elapsed_seconds() const -> double
    {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - start_time_)
                          .count();
        return static_cast<double>(ms) / 1000.0;
    }

  private:
    std::string operation_;
    std::vector<Package> packages_;
    bool is_tty_;
    bool color_enabled_;
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace kumi
