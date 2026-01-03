/// @file build_package.hpp
/// @brief Package build state tracking

#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace kumi::pkg {

/// @brief Represents the build state of a single package
struct BuildPackage final
{
    std::string name;    ///< Package name
    std::string version; ///< Package version

    /// @brief Build status states
    enum class Status : std::uint8_t
    {
        Pending,  ///< Waiting to build
        Building, ///< Currently building
        Complete, ///< Build completed successfully
        Cached,   ///< Using cached build artifact
    };

    Status status = Status::Pending;                  ///< Current build status
    std::chrono::steady_clock::time_point start_time; ///< Build start time
    std::chrono::milliseconds elapsed{0};             ///< Total build time
    std::string current_file;                         ///< Currently compiling file
    std::vector<std::string> completed_files;         ///< List of compiled files
};

} // namespace kumi::pkg
