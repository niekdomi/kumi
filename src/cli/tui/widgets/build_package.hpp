#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace kumi {

struct BuildPackage
{
    std::string name;
    std::string version;
    enum class Status
    {
        Pending,
        Building,
        Complete,
        Cached,
    };
    Status status = Status::Pending;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::milliseconds elapsed{0};

    std::string current_file;
    std::vector<std::string> completed_files;
};

} // namespace kumi
