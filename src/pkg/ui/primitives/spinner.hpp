/// @file spinner.hpp
/// @brief Animated spinner widget for terminal UI
///
/// Provides an animated spinner with status messages, timing information,
/// and support for success/error/warning states.

#pragma once

#include "cli/colors.hpp"
#include "ui/core/ansi.hpp"
#include "ui/widgets/common/animation.hpp"
#include "ui/widgets/common/symbols.hpp"
#include "ui/widgets/common/terminal_state.hpp"

#include <atomic>
#include <chrono>
#include <print>
#include <string>
#include <string_view>
#include <thread>

namespace kumi::pkg::ui {

/// @brief Animated spinner widget with status messages
///
/// Displays a rotating spinner animation with a message while work is in progress.
/// Supports updating the message, showing elapsed time, and terminating with
/// success/error/warning states.
///
/// Example usage:
/// ```cpp
/// Spinner spinner{"Loading data"};
/// spinner.start();
/// // ... do work ...
/// spinner.success("Data loaded successfully");
/// ```
class Spinner final
{
  public:
    /// @brief Constructs a spinner with a message
    /// @param message Initial status message
    /// @param term_state Terminal capabilities (auto-detected if not provided)
    explicit Spinner(std::string_view message = "Working",
                     kumi::ui::TerminalState term_state = kumi::ui::TerminalState{})
        : message_(message),
          term_state_(term_state)
    {}

    /// @brief Destructor ensures spinner is stopped
    ~Spinner()
    {
        stop();
    }

    // Non-copyable, non-movable
    Spinner(const Spinner&) = delete;
    auto operator=(const Spinner&) -> Spinner& = delete;
    Spinner(Spinner&&) = delete;
    auto operator=(Spinner&&) -> Spinner& = delete;

    /// @brief Starts the spinner animation
    ///
    /// If not connected to a TTY, prints a simple message instead of animating.
    /// Does nothing if already running.
    auto start() -> void
    {
        if (is_running_.load(std::memory_order_relaxed)) {
            return;
        }

        is_running_.store(true, std::memory_order_relaxed);
        timer_.reset();

        if (!term_state_.is_tty) {
            std::println("{}...", message_);
            return;
        }

        std::print("{}", ansi::CURSOR_HIDE);
        std::fflush(stdout);

        animation_thread_ = std::thread([this] { animate(); });
    }

    /// @brief Stops the spinner animation
    ///
    /// Cleans up the animation thread and restores cursor visibility.
    /// Does nothing if not running.
    auto stop() -> void
    {
        if (!is_running_.load(std::memory_order_relaxed)) {
            return;
        }

        is_running_.store(false, std::memory_order_relaxed);

        if (animation_thread_.joinable()) {
            animation_thread_.join();
        }

        if (term_state_.is_tty) {
            std::print("\r{}{}", ansi::CLEAR_LINE, ansi::CURSOR_SHOW);
            std::fflush(stdout);
        }
    }

    /// @brief Updates the spinner's message
    /// @param new_message New status message to display
    auto update_message(std::string_view new_message) -> void
    {
        message_ = new_message;
    }

    /// @brief Stops spinner and prints success message
    /// @param message Success message to display
    auto success(std::string_view message) -> void
    {
        stop();
        print_status(kumi::ui::symbols::SUCCESS, color::GREEN, message);
    }

    /// @brief Stops spinner and prints error message
    /// @param message Error message to display
    auto error(std::string_view message) -> void
    {
        stop();
        print_status(kumi::ui::symbols::ERROR, color::RED, message);
    }

    /// @brief Stops spinner and prints warning message
    /// @param message Warning message to display
    auto warning(std::string_view message) -> void
    {
        stop();
        print_status(kumi::ui::symbols::WARNING, color::YELLOW, message);
    }

    /// @brief Stops spinner and prints info message
    /// @param message Info message to display
    auto info(std::string_view message) -> void
    {
        stop();
        print_status(kumi::ui::symbols::ADDED, "", message);
    }

    /// @brief Gets elapsed time in milliseconds
    /// @return Milliseconds since start() was called
    [[nodiscard]]
    auto elapsed_ms() const noexcept -> long long
    {
        return timer_.elapsed_ms();
    }

    /// @brief Gets elapsed time in seconds
    /// @return Seconds since start() was called
    [[nodiscard]]
    auto elapsed_seconds() const noexcept -> double
    {
        return timer_.elapsed_seconds();
    }

  private:
    /// @brief Animation loop running in background thread
    auto animate() -> void
    {
        constexpr auto FRAME_COUNT = std::size(kumi::ui::symbols::SPINNER_DOTS);
        kumi::ui::AnimationController controller{FRAME_COUNT};

        while (is_running_.load(std::memory_order_relaxed)) {
            const auto frame = kumi::ui::symbols::SPINNER_DOTS[controller.current_frame()];
            const auto elapsed = elapsed_seconds();

            std::print("\r{}", ansi::CLEAR_LINE);
            std::print("{}{}{}{} {}...",
                       term_state_.color(color::CYAN),
                       frame,
                       term_state_.color(color::RESET),
                       message_);

            if (elapsed >= 2.0) {
                std::print(" {}{}({:.1f}s){}",
                           term_state_.color(color::DIM),
                           term_state_.color(color::YELLOW),
                           elapsed,
                           term_state_.color(color::RESET));
            }

            std::fflush(stdout);

            controller.advance();
            std::this_thread::sleep_for(controller.frame_duration());
        }
    }

    /// @brief Prints a status message with symbol and optional color
    /// @param symbol Unicode symbol to display
    /// @param status_color Color for the symbol (empty for no color)
    /// @param message Message text to display
    auto print_status(std::string_view symbol,
                      std::string_view status_color,
                      std::string_view message) const -> void
    {
        std::println("{}{}{}{} {}",
                     term_state_.color(status_color),
                     symbol,
                     term_state_.color(color::RESET),
                     message);
    }

    std::string message_;
    kumi::ui::TerminalState term_state_;
    kumi::ui::ElapsedTimer timer_;
    kumi::ui::AnimationController animation_controller_{std::size(kumi::ui::symbols::SPINNER_DOTS)};
    std::atomic<bool> is_running_{false};
    std::thread animation_thread_;
};

} // namespace kumi::pkg::ui
