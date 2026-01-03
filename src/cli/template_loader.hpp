#pragma once

#include <expected>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace kumi {

namespace fs = std::filesystem;

class TemplateLoader
{
  public:
    [[nodiscard]]
    static auto load_template(std::string_view template_name)
      -> std::expected<std::string, std::string>
    {
        const fs::path template_path = get_template_dir() / template_name;

        std::ifstream file(template_path);
        if (!file) {
            return std::unexpected("Failed to open template: " + std::string(template_name));
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    [[nodiscard]]
    static auto substitute(std::string_view template_content,
                           const std::map<std::string, std::string>& variables) -> std::string
    {
        std::string result(template_content);

        for (const auto& [key, value] : variables) {
            const std::string placeholder = "{" + key + "}";
            size_t pos = 0;
            while ((pos = result.find(placeholder, pos)) != std::string::npos) {
                result.replace(pos, placeholder.length(), value);
                pos += value.length();
            }
        }

        return result;
    }

  private:
    [[nodiscard]]
    static auto get_template_dir() -> fs::path
    {
        return fs::path(__FILE__).parent_path() / "templates";
    }
};

} // namespace kumi
