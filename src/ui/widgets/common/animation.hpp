/// @file animation.hpp
/// @brief Frame-based animation utilities for terminal UI
///
/// Provides thread-safe animation state management for spinners and
/// other animated UI widgets.

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <string_view>

namespace kumi::ui {

/// @brief Frame-based animation controller
///
/// Manages animation frame indices and timing for smooth terminal animations.
/// Thread-safe for use with concurrent rendering.
class AnimationController final
{
  public:
    /// @brief Constructs an animation controller
    /// @param frame_count Number of frames in the animation
    /// @param frame_duration Duration to show each frame
    explicit constexpr AnimationController(
      std::size_t frame_count,
      std::chrono::milliseconds frame_duration = std::chrono::milliseconds{80}) noexcept
        : frame_count_(frame_count),
          frame_duration_(frame_duration)
    {}

    /// @brief Gets the current frame index
    /// @return Current frame index (0 to frame_count - 1)
    [[nodiscard]]
    auto current_frame() const noexcept -> std::size_t
    {
        return frame_index_.load(std::memory_order_relaxed);
    }

    /// @brief Advances to the next frame
    /// @return New frame index after advancing
    auto advance() noexcept -> std::size_t
    {
        const auto current = frame_index_.load(std::memory_order_relaxed);
        const auto next = (current + 1) % frame_count_;
        frame_index_.store(next, std::memory_order_relaxed);
        return next;
    }

    /// @brief Resets the animation to the first frame
    auto reset() noexcept -> void
    {
        frame_index_.store(0, std::memory_order_relaxed);
    }

    /// @brief Gets the frame duration
    /// @return Duration each frame should be displayed
    [[nodiscard]]
    constexpr auto frame_duration() const noexcept -> std::chrono::milliseconds
    {
        return frame_duration_;
    }

  private:
    std::size_t frame_count_;
    std::chrono::milliseconds frame_duration_;
    std::atomic<std::size_t> frame_index_{0};
};

/// @brief Elapsed time tracker
///
/// Tracks elapsed time since a starting point. Useful for showing
/// duration information in progress indicators.
class ElapsedTimer final
{
  public:
    /// @brief Constructs a timer starting at the current time
    ElapsedTimer() noexcept : start_time_(std::chrono::steady_clock::now()) {}

    /// @brief Resets the timer to the current time
    auto reset() noexcept -> void
    {
        start_time_ = std::chrono::steady_clock::now();
    }

    /// @brief Gets elapsed time in milliseconds
    /// @return Milliseconds since timer start
    [[nodiscard]]
    auto elapsed_ms() const noexcept -> long long
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - start_time_)
          .count();
    }

    /// @brief Gets elapsed time in seconds
    /// @return Seconds since timer start
    [[nodiscard]]
    auto elapsed_seconds() const noexcept -> double
    {
        return static_cast<double>(elapsed_ms()) / 1000.0;
    }

  private:
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace kumi::ui
