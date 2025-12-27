#pragma once

#include "progress.hpp"
#include "terminal.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <vector>

namespace kumi {

// Build status for a single package
struct BuildPackage
{
    std::string name;
    std::string version;
    enum class Status
    {
        Pending,
        Building,
        Complete,
        Cached
    };
    Status status = Status::Pending;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::milliseconds elapsed{ 0 };

    // File being compiled (optional, for detailed view)
    std::string current_file;
    std::vector<std::string> completed_files;
};

// Build status tracker showing parallel builds
class BuildStatusTracker
{
  public:
    BuildStatusTracker() :
      is_running_(false),
      is_tty_(isatty(STDOUT_FILENO) != 0),
      color_enabled_(is_tty_ && (std::getenv("NO_COLOR") == nullptr)),
      frame_index_(0)
    {
    }

    ~BuildStatusTracker() { stop(); }

    // Non-copyable, non-movable
    BuildStatusTracker(const BuildStatusTracker &) = delete;
    auto operator=(const BuildStatusTracker &) -> BuildStatusTracker & = delete;
    BuildStatusTracker(BuildStatusTracker &&) = delete;
    auto operator=(BuildStatusTracker &&) -> BuildStatusTracker & = delete;

    void add_package(std::string_view name, std::string_view version, bool cached = false)
    {
        std::lock_guard lock(mutex_);
        BuildPackage pkg;
        pkg.name = name;
        pkg.version = version;
        pkg.status = cached ? BuildPackage::Status::Cached : BuildPackage::Status::Pending;
        packages_.push_back(std::move(pkg));
    }

    void start_building(std::string_view name)
    {
        std::lock_guard lock(mutex_);
        for (auto &pkg : packages_) {
            if (pkg.name == name && pkg.status == BuildPackage::Status::Pending) {
                pkg.status = BuildPackage::Status::Building;
                pkg.start_time = std::chrono::steady_clock::now();
                break;
            }
        }
    }

    void update_file(std::string_view name, std::string_view file)
    {
        std::lock_guard lock(mutex_);
        for (auto &pkg : packages_) {
            if (pkg.name == name && pkg.status == BuildPackage::Status::Building) {
                if (!pkg.current_file.empty()) {
                    pkg.completed_files.push_back(pkg.current_file);
                }
                pkg.current_file = file;
                break;
            }
        }
    }

    void complete_package(std::string_view name)
    {
        std::lock_guard lock(mutex_);
        for (auto &pkg : packages_) {
            if (pkg.name == name && pkg.status == BuildPackage::Status::Building) {
                pkg.status = BuildPackage::Status::Complete;
                pkg.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - pkg.start_time);
                pkg.current_file.clear();
                break;
            }
        }
    }

    void start()
    {
        if (is_running_.load()) {
            return;
        }

        is_running_.store(true);

        if (!is_tty_) {
            std::println("Building packages...");
            return;
        }

        std::println("Building packages...");
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
            std::print("{}", ansi::CURSOR_SHOW);
            static_cast<void>(std::fflush(stdout));
        }
    }

  private:
    static constexpr std::array<std::string_view, 10> SPINNER_FRAMES
      = { "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏" };

    void animate()
    {
        while (is_running_.load()) {
            render_status();
            frame_index_ = (frame_index_ + 1) % SPINNER_FRAMES.size();
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
    }

    void render_status()
    {
        std::lock_guard lock(mutex_);

        // Find longest package name for alignment
        size_t max_name_len = 0;
        for (const auto &pkg : packages_) {
            max_name_len = std::max(max_name_len, pkg.name.length());
        }

        // Move cursor up to overwrite previous output
        if (packages_.size() > 0 && frame_index_ > 0) {
            std::print("{}", ansi::move_up(static_cast<int>(packages_.size())));
        }

        for (const auto &pkg : packages_) {
            std::print("\r{}", ansi::CLEAR_LINE);

            if (pkg.status == BuildPackage::Status::Complete) {
                // Completed package
                if (color_enabled_) {
                    std::println(" {}{}{} {:<{}} {}v{}{} {}({:.0f}ms){}",
                                 ansi::GREEN,
                                 symbols::SUCCESS,
                                 ansi::RESET,
                                 pkg.name,
                                 max_name_len,
                                 ansi::DIM,
                                 pkg.version,
                                 ansi::RESET,
                                 ansi::DIM,
                                 static_cast<double>(pkg.elapsed.count()),
                                 ansi::RESET);
                } else {
                    std::println(" {} {:<{}} v{} ({:.0f}ms)",
                                 symbols::SUCCESS,
                                 pkg.name,
                                 max_name_len,
                                 pkg.version,
                                 static_cast<double>(pkg.elapsed.count()));
                }
            } else if (pkg.status == BuildPackage::Status::Cached) {
                // Cached package
                if (color_enabled_) {
                    std::println(" {}{}{} {:<{}} {}v{}{} (cached)",
                                 ansi::GREEN,
                                 symbols::SUCCESS,
                                 ansi::RESET,
                                 pkg.name,
                                 max_name_len,
                                 ansi::DIM,
                                 pkg.version,
                                 ansi::RESET);
                } else {
                    std::println(" {} {:<{}} v{} (cached)",
                                 symbols::SUCCESS,
                                 pkg.name,
                                 max_name_len,
                                 pkg.version);
                }
            } else if (pkg.status == BuildPackage::Status::Building) {
                // Currently building
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - pkg.start_time);
                const auto frame = SPINNER_FRAMES[frame_index_];

                if (color_enabled_) {
                    std::println(" {}{}{} {:<{}} {}v{}{} {}({:.1f}s){}",
                                 ansi::CYAN,
                                 frame,
                                 ansi::RESET,
                                 pkg.name,
                                 max_name_len,
                                 ansi::DIM,
                                 pkg.version,
                                 ansi::RESET,
                                 ansi::DIM,
                                 elapsed.count() / 1000.0,
                                 ansi::RESET);
                } else {
                    std::println(" {} {:<{}} v{} ({:.1f}s)",
                                 frame,
                                 pkg.name,
                                 max_name_len,
                                 pkg.version,
                                 elapsed.count() / 1000.0);
                }

                // Show current file being compiled (if detailed mode)
                if (!pkg.current_file.empty()) {
                    const auto file_elapsed = std::chrono::milliseconds(320); // Simulated
                    if (color_enabled_) {
                        std::println("   {}└─ {} compiling {} {}({:.0f}ms){}",
                                     ansi::DIM,
                                     frame,
                                     pkg.current_file,
                                     ansi::DIM,
                                     static_cast<double>(file_elapsed.count()),
                                     ansi::RESET);
                    } else {
                        std::println("   └─ {} compiling {} ({:.0f}ms)",
                                     frame,
                                     pkg.current_file,
                                     static_cast<double>(file_elapsed.count()));
                    }
                }
            }
        }

        static_cast<void>(std::fflush(stdout));
    }

    std::vector<BuildPackage> packages_;
    std::mutex mutex_;
    std::atomic<bool> is_running_;
    bool is_tty_;
    bool color_enabled_;
    size_t frame_index_;
    std::thread animation_thread_;
};

} // namespace kumi
