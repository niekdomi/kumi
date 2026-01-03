#pragma once

#include "ui/core/ansi.hpp"
#include "ui/widgets/select.hpp"
#include "ui/widgets/text_input.hpp"

#include <functional>
#include <map>
#include <print>
#include <string>

namespace kumi::cli {

class PromptSession
{
  public:
    void add_text_input(std::string_view key, std::string_view prompt, std::string placeholder = "")
    {
        prompts_.emplace_back([prompt, placeholder, key, this]() -> void {
            ui::TextInput input(prompt, placeholder);
            answers_[std::string(key)] = input.run();
        });
    }

    void add_validated_text_input(std::string_view key,
                                  std::string_view prompt,
                                  const std::string& placeholder = "")
    {
        prompts_.emplace_back([prompt, placeholder, key, this]() -> void {
            while (true) {
                ui::TextInput input(prompt, placeholder);
                std::string result = input.run();

                if (result.empty() || result.find_first_not_of(" \t\n\r") == std::string::npos) {
                    std::print("{}Error:{} Project name cannot only contain whitespace\n",
                               ansi::RED,
                               ansi::RESET);
                    continue;
                }

                const std::string invalid_chars = "/\\:*?\"<>|\0";
                if (result.find_first_of(invalid_chars) != std::string::npos) {
                    std::print("{}Error:{} Project name contains invalid characters\n",
                               ansi::RED,
                               ansi::RESET);
                    continue;
                }

                if (result == "." || result == "..") {
                    std::print(
                      "{}Error:{} Project name cannot be '.' or '..'\n", ansi::RED, ansi::RESET);
                    continue;
                }

                answers_[std::string(key)] = result;
                break;
            }
        });
    }

    void add_select(std::string_view key,
                    std::string_view prompt,
                    std::vector<std::string> options,
                    unsigned int default_index)
    {
        prompts_.emplace_back(
          [prompt, default_index, key, this, opts = std::move(options)]() mutable -> void {
              ui::Select select(prompt, std::move(opts), default_index);
              answers_[std::string(key)] = select.run();
          });
    }

    void add_yes_no(std::string_view key, std::string_view prompt, bool default_yes = true)
    {
        prompts_.emplace_back([prompt, default_yes, key, this]() -> void {
            ui::Select select(prompt, {"yes", "no"}, default_yes ? 0 : 1);
            answers_[std::string(key)] = select.run();
        });
    }

    void run()
    {
        for (auto& prompt : prompts_) {
            prompt();
        }
    }

    [[nodiscard]]
    auto get(std::string_view key) const -> std::string
    {
        auto it = answers_.find(std::string(key));
        if (it != answers_.end()) {
            return it->second;
        }
        return "";
    }

    [[nodiscard]]
    auto get_all() const -> const std::map<std::string, std::string>&
    {
        return answers_;
    }

    [[nodiscard]]
    auto is_yes(std::string_view key) const -> bool
    {
        return get(key) == "yes";
    }

  private:
    std::vector<std::function<void()>> prompts_;
    std::map<std::string, std::string> answers_;
};

} // namespace kumi::cli
