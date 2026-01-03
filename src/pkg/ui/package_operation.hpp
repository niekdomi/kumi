/// @file package_operation.hpp
/// @brief Package installation/operation summary display

#pragma once

#include "cli/colors.hpp"
#include "ui/widgets/common/symbols.hpp"
#include "ui/widgets/common/terminal_state.hpp"

#include <algorithm>
#include <chrono>
#include <print>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace kumi::pkg::ui {

/// @brief Tracks and displays package installation operations
///
/// Shows a hierarchical view of installed packages with root packages
/// and their transitive dependencies.
class PackageOperationTracker final
{
  public:
    /// @brief Represents a package in the operation
    struct Package final
    {
        std::string name;
        std::string version;
        bool cached = false;
        bool is_root = false;
        size_t size_bytes = 0;
    };

    explicit PackageOperationTracker(std::string_view operation = "Installing",
                                     kumi::ui::TerminalState term_state = kumi::ui::TerminalState{})
        : operation_(operation),
          term_state_(term_state),
          start_time_(std::chrono::steady_clock::now())
    {}

    void add_package(Package pkg)
    {
        packages_.push_back(std::move(pkg));
    }

    void show_summary()
    {
        size_t max_name_len = 0;
        for (const auto& pkg : packages_) {
            max_name_len = std::max(max_name_len, pkg.name.length());
        }

        std::vector<Package> root_packages;
        std::vector<Package> transitive_deps;

        for (const auto& pkg : packages_) {
            if (pkg.is_root) {
                root_packages.push_back(pkg);
            } else {
                transitive_deps.push_back(pkg);
            }
        }

        for (size_t i = 0; i < root_packages.size(); ++i) {
            const auto& pkg = root_packages[i];
            const bool is_last_root = (i == root_packages.size() - 1);

            std::println(" {}{}{}{} {:<{}} {}v{}{}{}",
                         term_state_.color(color::GREEN),
                         kumi::ui::symbols::ADDED,
                         term_state_.color(color::RESET),
                         pkg.name,
                         max_name_len,
                         term_state_.color(color::DIM),
                         pkg.version,
                         term_state_.color(color::RESET),
                         pkg.cached ? " (cached)" : "");

            if (!transitive_deps.empty() && is_last_root) {
                for (size_t j = 0; j < transitive_deps.size(); ++j) {
                    const auto& dep = transitive_deps[j];
                    const bool is_last = (j == transitive_deps.size() - 1);
                    const std::string_view tree_char = is_last ? "└─" : "├─";

                    std::println("   {}{} {:<{}} {}v{}{}{}",
                                 term_state_.color(color::DIM),
                                 tree_char,
                                 dep.name,
                                 max_name_len,
                                 term_state_.color(color::DIM),
                                 dep.version,
                                 term_state_.color(color::RESET),
                                 dep.cached ? " (cached)" : "");
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
    kumi::ui::TerminalState term_state_;
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace kumi::pkg::ui
