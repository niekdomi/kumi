#pragma once

#include "cli/colors.hpp"
#include "cli/tui/core/ansi.hpp"
#include "cli/tui/widgets/spinner.hpp"

#include <algorithm>
#include <chrono>
#include <print>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace kumi {

class PackageOperationTracker
{
  public:
    struct Package
    {
        std::string name;
        std::string version;
        bool cached = false;
        bool is_root = false;
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
        size_t max_name_len = 0;
        for (const auto &pkg : packages_) {
            max_name_len = std::max(max_name_len, pkg.name.length());
        }

        std::vector<Package> root_packages;
        std::vector<Package> transitive_deps;

        for (const auto &pkg : packages_) {
            if (pkg.is_root) {
                root_packages.push_back(pkg);
            } else {
                transitive_deps.push_back(pkg);
            }
        }

        for (size_t i = 0; i < root_packages.size(); ++i) {
            const auto &pkg = root_packages[i];
            const bool is_last_root = (i == root_packages.size() - 1);

            if (color_enabled_) {
                std::println(" {}{}{} {:<{}} {}v{}{}{}",
                             color::GREEN,
                             symbols::ADDED,
                             color::RESET,
                             pkg.name,
                             max_name_len,
                             color::DIM,
                             pkg.version,
                             color::RESET,
                             pkg.cached ? " (cached)" : "");
            } else {
                std::println(" {} {:<{}} v{}{}",
                             symbols::ADDED,
                             pkg.name,
                             max_name_len,
                             pkg.version,
                             pkg.cached ? " (cached)" : "");
            }

            if (!transitive_deps.empty() && is_last_root) {
                for (size_t j = 0; j < transitive_deps.size(); ++j) {
                    const auto &dep = transitive_deps[j];
                    const bool is_last = (j == transitive_deps.size() - 1);
                    const auto tree_char = is_last ? "└─" : "├─";

                    if (color_enabled_) {
                        std::println("   {}{} {:<{}} {}v{}{}{}",
                                     color::DIM,
                                     tree_char,
                                     dep.name,
                                     max_name_len,
                                     color::DIM,
                                     dep.version,
                                     color::RESET,
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
