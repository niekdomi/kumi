#pragma once

#include "cli/template_loader.hpp"

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace kumi {

namespace fs = std::filesystem;

class ProjectGenerator
{
  public:
    struct Config
    {
        std::string name;
        std::string type;         // "executable", "library", "header-only"
        std::string cpp_standard; // "17", "20", "23"
        bool use_git;
    };

    explicit ProjectGenerator(Config config) : config_(std::move(config)) {}

    auto generate(const fs::path &base_path = fs::current_path())
      -> std::expected<void, std::string>
    {
        project_path_ = base_path / config_.name;

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

            generate_kumi_build_file();
            generate_gitignore();

            if (config_.type == "executable") {
                generate_main_cpp();
            } else if (config_.type == "library") {
                generate_library_files();
            } else if (config_.type == "header-only") {
                generate_header_only_files();
            }

            if (config_.use_git) {
                initialize_git();
            }
        }
        catch (const std::exception &e) {
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
        auto template_result = TemplateLoader::load_template("kumi_build.template");
        if (!template_result) {
            throw std::runtime_error(template_result.error());
        }

        std::map<std::string, std::string> vars = {
          {"PROJECT_NAME", config_.name        },
          {"TARGET_TYPE",  config_.type        },
          {"CPP_STANDARD", config_.cpp_standard}
        };

        if (config_.type == "library") {
            vars["HEADERS_LINE"] =
              std::format("  headers: \"include/{}/**/*.hpp\";\n", config_.name);
        } else if (config_.type == "header-only") {
            vars["HEADERS_LINE"] =
              std::format("  headers: \"include/{}/**/*.hpp\";\n", config_.name);
        } else {
            vars["HEADERS_LINE"] = "";
        }

        const auto content = TemplateLoader::substitute(*template_result, vars);

        std::ofstream file(project_path_ / "kumi.build");
        file << content;
    }

    void generate_gitignore()
    {
        auto template_result = TemplateLoader::load_template("gitignore.template");
        if (!template_result) {
            throw std::runtime_error(template_result.error());
        }

        std::ofstream file(project_path_ / ".gitignore");
        file << *template_result;
    }

    void generate_main_cpp()
    {
        const auto template_name =
          (config_.cpp_standard == "23") ? "main_cpp23.template" : "main_cpp_legacy.template";

        auto template_result = TemplateLoader::load_template(template_name);
        if (!template_result) {
            throw std::runtime_error(template_result.error());
        }

        std::map<std::string, std::string> vars = {
          {"PROJECT_NAME", config_.name}
        };

        const auto content = TemplateLoader::substitute(*template_result, vars);

        std::ofstream file(project_path_ / "src" / "main.cpp");
        file << content;
    }

    void generate_library_files()
    {
        auto header_template = TemplateLoader::load_template("library_header.template");
        auto source_template = TemplateLoader::load_template("library_source.template");

        if (!header_template || !source_template) {
            throw std::runtime_error("Failed to load library templates");
        }

        std::map<std::string, std::string> vars = {
          {"PROJECT_NAME", config_.name}
        };

        const auto header_content = TemplateLoader::substitute(*header_template, vars);
        const auto source_content = TemplateLoader::substitute(*source_template, vars);

        std::ofstream header(project_path_ / "include" / config_.name / (config_.name + ".hpp"));
        header << header_content;

        std::ofstream source(project_path_ / "src" / (config_.name + ".cpp"));
        source << source_content;
    }

    void generate_header_only_files()
    {
        auto template_result = TemplateLoader::load_template("library_header.template");
        if (!template_result) {
            throw std::runtime_error(template_result.error());
        }

        std::map<std::string, std::string> vars = {
          {"PROJECT_NAME", config_.name}
        };

        const auto content = TemplateLoader::substitute(*template_result, vars);

        std::ofstream header(project_path_ / "include" / config_.name / (config_.name + ".hpp"));
        header << content;
    }

    void initialize_git()
    {
        std::string command =
          std::format("cd {} && git init > /dev/null 2>&1", project_path_.string());
        std::system(command.c_str());
    }

    Config config_;
    fs::path project_path_;
};

} // namespace kumi
