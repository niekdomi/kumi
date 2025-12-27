#pragma once

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace kumi {

namespace fs = std::filesystem;

// Project generator for creating new C++ projects
class ProjectGenerator
{
  public:
    struct Config
    {
        std::string name;
        std::string type;         // "executable", "library", "header-only"
        std::string cpp_standard; // "17", "20", "23"
        std::string license;      // "MIT", "Apache-2.0", "GPL-3.0"
        bool use_git;
    };

    explicit ProjectGenerator(Config config) : config_(std::move(config)) {}

    // Generate the project
    auto generate(const fs::path &base_path = fs::current_path())
      -> std::expected<void, std::string>
    {
        project_path_ = base_path / config_.name;

        // Create project directory
        if (fs::exists(project_path_)) {
            return std::unexpected(std::format("Directory '{}' already exists", config_.name));
        }

        try {
            fs::create_directories(project_path_);

            if (config_.type == "executable") {
                fs::create_directories(project_path_ / "src");
            } else if (config_.type == "library") {
                fs::create_directories(project_path_ / "src");
                fs::create_directories(project_path_ / "include" / config_.name);
            } else if (config_.type == "header-only") {
                fs::create_directories(project_path_ / "include" / config_.name);
            }

            // Generate files
            generate_kumi_build_file();
            if (config_.license != "None") {
                generate_license();
            }
            generate_gitignore();

            if (config_.type == "executable") {
                generate_main_cpp();
            } else if (config_.type == "library") {
                generate_library_files();
            } else if (config_.type == "header-only") {
                generate_header_only_files();
            }

            // Initialize git repository
            if (config_.use_git) {
                initialize_git();
            }

        } catch (const std::exception &e) {
            return std::unexpected(std::format("Failed to generate project: {}", e.what()));
        }

        return {};
    }

    [[nodiscard]]
    auto get_project_path() const -> fs::path
    {
        return project_path_;
    }

  private:
    void generate_kumi_build_file()
    {
        auto path = project_path_ / "kumi.build";
        std::ofstream file(path);

        // Generate project section
        file << std::format("project {} {{\n", config_.name);
        file << "  version: \"0.1.0\";\n";
        if (config_.license != "None") {
            file << std::format("  license: \"{}\";\n", config_.license);
        }
        file << "}\n\n";

        // Generate target section
        file << std::format("target {} {{\n", config_.name);

        if (config_.type == "executable") {
            file << "  type: executable;\n";
            file << "  sources: \"src/**/*.cpp\";\n";
        } else if (config_.type == "library") {
            file << "  type: library;\n";
            file << "  sources: \"src/**/*.cpp\";\n";
            file << std::format("  headers: \"include/{}/**/*.hpp\";\n", config_.name);
        } else if (config_.type == "header-only") {
            file << "  type: header-only;\n";
            file << std::format("  headers: \"include/{}/**/*.hpp\";\n", config_.name);
        }

        file << "\n  public {\n";
        file << std::format("    cpp-standard: \"{}\";\n", config_.cpp_standard);
        file << "  }\n";
        file << "}\n";
    }

    void generate_license()
    {
        auto path = project_path_ / "LICENSE";
        std::ofstream file(path);

        if (config_.license == "MIT") {
            file << R"(MIT License

Copyright (c) 2025

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
)";
        } else if (config_.license == "Apache-2.0") {
            file << R"(Apache License
Version 2.0, January 2004
http://www.apache.org/licenses/

[Full Apache 2.0 license text would go here]
)";
        } else if (config_.license == "GPL-3.0") {
            file << R"(GNU GENERAL PUBLIC LICENSE
Version 3, 29 June 2007

[Full GPL 3.0 license text would go here]
)";
        }
    }

    void generate_gitignore()
    {
        auto path = project_path_ / ".gitignore";
        std::ofstream file(path);

        file << R"(# Xmake
.xmake/
build/

# Build artifacts
*.o
*.a
*.so
*.dylib
*.exe

# IDE
.vscode/
.idea/
*.swp
*.swo
*~

# OS
.DS_Store
Thumbs.db
)";
    }

    void generate_main_cpp()
    {
        auto path = project_path_ / "src" / "main.cpp";
        std::ofstream file(path);

        file << std::format(R"(#include <print>

int main() {{
    std::println("Hello from {}!");
    return 0;
}}
)",
                            config_.name);
    }

    void generate_library_files()
    {
        // Generate header
        auto header_path = project_path_ / "include" / config_.name / (config_.name + ".hpp");
        std::ofstream header(header_path);

        header << std::format(R"(#pragma once

#include <string>

namespace {} {{

class Example {{
public:
    Example() = default;
    
    std::string greet() const {{
        return "Hello from {}!";
    }}
}};

}} // namespace {}
)",
                              config_.name,
                              config_.name,
                              config_.name);

        // Generate source
        auto src_path = project_path_ / "src" / (config_.name + ".cpp");
        std::ofstream src(src_path);

        src << std::format(R"(#include "{}/{}.hpp"

namespace {} {{

// Implementation goes here

}} // namespace {}
)",
                           config_.name,
                           config_.name,
                           config_.name,
                           config_.name);
    }

    void generate_header_only_files()
    {
        auto header_path = project_path_ / "include" / config_.name / (config_.name + ".hpp");
        std::ofstream header(header_path);

        header << std::format(R"(#pragma once

#include <string>

namespace {} {{

class Example {{
public:
    Example() = default;
    
    std::string greet() const {{
        return "Hello from {}!";
    }}
}};

}} // namespace {}
)",
                              config_.name,
                              config_.name,
                              config_.name);
    }

    void initialize_git()
    {
        // Run git init in the project directory, silencing output
        std::string command
          = std::format("cd {} && git init > /dev/null 2>&1", project_path_.string());
        std::system(command.c_str());
    }

    Config config_;
    fs::path project_path_;
};

} // namespace kumi
