/// @file build_status_tracker.hpp
/// @brief Multi-package build status display

#pragma once

#include "cli/colors.hpp"
#include "pkg/ui/build_package.hpp"
#include "pkg/ui/primitives/spinner.hpp"
#include "ui/core/ansi.hpp"
#include "ui/widgets/common/symbols.hpp"
#include "ui/widgets/common/terminal_state.hpp"

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

namespace kumi::pkg::ui {

/// @brief Tracks and displays build status for multiple packages
///
/// Shows real-time progress of parallel package builds with spinners,
/// elapsed time, and file-level compilation progress.
class BuildStatusTracker final
{
  public:
    BuildStatusTracker(kumi::ui::TerminalState term_state = kumi::ui::TerminalState{})
        : term_state_(term_state)
    {}

    ~BuildStatusTracker()
    {
        stop();
    }

    BuildStatusTracker(const BuildStatusTracker&) = delete;
    auto operator=(const BuildStatusTracker&) -> BuildStatusTracker& = delete;
    BuildStatusTracker(BuildStatusTracker&&) = delete;
    auto operator=(BuildStatusTracker&&) -> BuildStatusTracker& = delete;

    void add_package(std::string_view name, std::string_view version, bool cached = false)
    {
        std::lock_guard lock(mutex_);
        pkg::BuildPackage pkg;
        pkg.name = name;
        pkg.version = version;
        pkg.status =
          cached ? pkg::BuildPackage::Status::Cached : pkg::BuildPackage::Status::Pending;
        packages_.push_back(std::move(pkg));
    }

    void start_building(std::string_view name)
    {
        std::lock_guard lock(mutex_);
        for (auto& pkg : packages_) {
            if (pkg.name == name && pkg.status == pkg::BuildPackage::Status::Pending) {
                pkg.status = pkg::BuildPackage::Status::Building;
                pkg.start_time = std::chrono::steady_clock::now();
                break;
            }
        }
    }

    void update_file(std::string_view name, std::string_view file)
    {
        std::lock_guard lock(mutex_);
        for (auto& pkg : packages_) {
            if (pkg.name == name && pkg.status == pkg::BuildPackage::Status::Building) {
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
        for (auto& pkg : packages_) {
            if (pkg.name == name && pkg.status == pkg::BuildPackage::Status::Building) {
                pkg.status = pkg::BuildPackage::Status::Complete;
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

        if (!term_state_.is_tty) {
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

        if (term_state_.is_tty) {
            std::print("{}", ansi::CURSOR_SHOW);
            static_cast<void>(std::fflush(stdout));
        }
    }

  private:
    void animate()
    {
        while (is_running_.load()) {
            render_status();
            frame_index_ = (frame_index_ + 1) % std::size(kumi::ui::symbols::SPINNER_DOTS);
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
    }

    void render_status()
    {
        std::lock_guard lock(mutex_);

        size_t max_name_len = 0;
        for (const auto& pkg : packages_) {
            max_name_len = std::max(max_name_len, pkg.name.length());
        }

        if (packages_.size() > 0 && frame_index_ > 0) {
            std::print("{}", ansi::move_up(static_cast<int>(packages_.size())));
        }

        for (const auto& pkg : packages_) {
            std::print("\r{}", ansi::CLEAR_LINE);

            if (pkg.status == pkg::BuildPackage::Status::Complete) {
                std::println(" {}{}{}{} {:<{}} {}v{}{} {}({:.0f}ms){}",
                             term_state_.color(color::GREEN),
                             kumi::ui::symbols::SUCCESS,
                             term_state_.color(color::RESET),
                             pkg.name,
                             max_name_len,
                             term_state_.color(color::DIM),
                             pkg.version,
                             term_state_.color(color::RESET),
                             term_state_.color(color::DIM),
                             static_cast<double>(pkg.elapsed.count()),
                             term_state_.color(color::RESET));
            } else if (pkg.status == pkg::BuildPackage::Status::Cached) {
                std::println(" {}{}{}{} {:<{}} {}v{}{} (cached)",
                             term_state_.color(color::GREEN),
                             kumi::ui::symbols::SUCCESS,
                             term_state_.color(color::RESET),
                             pkg.name,
                             max_name_len,
                             term_state_.color(color::DIM),
                             pkg.version,
                             term_state_.color(color::RESET));
            } else if (pkg.status == pkg::BuildPackage::Status::Building) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - pkg.start_time);
                const auto frame = kumi::ui::symbols::SPINNER_DOTS[frame_index_];

                std::println(" {}{}{}{} {:<{}} {}v{}{} {}({:.1f}s){}",
                             term_state_.color(color::CYAN),
                             frame,
                             term_state_.color(color::RESET),
                             pkg.name,
                             max_name_len,
                             term_state_.color(color::DIM),
                             pkg.version,
                             term_state_.color(color::RESET),
                             term_state_.color(color::DIM),
                             elapsed.count() / 1000.0,
                             term_state_.color(color::RESET));

                if (!pkg.current_file.empty()) {
                    std::println("   {}└─ {} compiling {}{}",
                                 term_state_.color(color::DIM),
                                 frame,
                                 pkg.current_file,
                                 term_state_.color(color::RESET));
                }
            }
        }

        static_cast<void>(std::fflush(stdout));
    }

    std::vector<pkg::BuildPackage> packages_;
    std::mutex mutex_;
    std::atomic<bool> is_running_{false};
    kumi::ui::TerminalState term_state_;
    size_t frame_index_;
    std::thread animation_thread_;
};

} // namespace kumi::pkg::ui
