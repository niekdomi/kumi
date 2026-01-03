#pragma once

#include "cli/colors.hpp"
#include "cli/tui/core/ansi.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>

namespace kumi {

namespace spinner_frames {

constexpr std::array<std::string_view, 10> DOTS =
  {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};

} // namespace spinner_frames

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

class Spinner
{
  public:
    explicit Spinner(std::string_view message = "Working")
        : message_(message),
          is_running_(false),
          is_tty_(isatty(STDOUT_FILENO) != 0),
          color_enabled_(is_tty_ && (std::getenv("NO_COLOR") == nullptr)),
          start_time_(std::chrono::steady_clock::now())
    {}

    ~Spinner()
    {
        stop();
    }

    Spinner(const Spinner&) = delete;
    auto operator=(const Spinner&) -> Spinner& = delete;
    Spinner(Spinner&&) = delete;
    auto operator=(Spinner&&) -> Spinner& = delete;

    void start()
    {
        if (is_running_.load()) {
            return;
        }

        is_running_.store(true);
        start_time_ = std::chrono::steady_clock::now();

        if (!is_tty_) {
            std::println("{}...", message_);
            return;
        }

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
            std::print("\r{}", ansi::CLEAR_LINE);
            std::print("{}", ansi::CURSOR_SHOW);
            static_cast<void>(std::fflush(stdout));
        }
    }

    void update_message(std::string_view new_message)
    {
        message_ = new_message;
    }

    void success(std::string_view message)
    {
        stop();
        if (color_enabled_) {
            std::println("{}{}{} {}", color::GREEN, symbols::SUCCESS, color::RESET, message);
        } else {
            std::println("{} {}", symbols::SUCCESS, message);
        }
    }

    void success_with_bold(std::string_view prefix,
                           std::string_view bold_word,
                           std::string_view suffix)
    {
        stop();
        if (color_enabled_) {
            std::println("{}{}{} {}{}{}{}{}{}{}",
                         color::GREEN,
                         symbols::SUCCESS,
                         color::RESET,
                         color::DIM,
                         prefix,
                         color::BOLD,
                         bold_word,
                         color::RESET,
                         color::DIM,
                         suffix,
                         color::RESET);
        } else {
            std::println("{} {}{}{}", symbols::SUCCESS, prefix, bold_word, suffix);
        }
    }

    void error(std::string_view message)
    {
        stop();
        if (color_enabled_) {
            std::println("{}{}{} {}", color::RED, symbols::ERROR, color::RESET, message);
        } else {
            std::println("{} {}", symbols::ERROR, message);
        }
    }

    void warning(std::string_view message)
    {
        stop();
        if (color_enabled_) {
            std::println("{}{}{} {}", color::YELLOW, symbols::WARNING, color::RESET, message);
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
    size_t frame_index_{0};
    std::chrono::steady_clock::time_point start_time_;
    std::thread animation_thread_;
};

} // namespace kumi
