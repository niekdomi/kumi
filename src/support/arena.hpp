#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

namespace kumi {

class Arena
{
    std::vector<std::unique_ptr<char[]>> blocks_;
    char* current_{nullptr};
    size_t remaining_{0};
    static constexpr size_t BLOCK_SIZE = static_cast<const size_t>(64 * 1'024);

  public:
    constexpr Arena() = default;

    Arena(const Arena&) = delete;
    Arena(Arena&&) noexcept = default;

    auto operator=(const Arena&) -> Arena& = delete;
    auto operator=(Arena&&) noexcept -> Arena& = default;

    [[nodiscard]]
    auto store(std::string_view str) -> std::string_view
    {
        if (str.empty()) {
            return {};
        }

        if (str.size() > remaining_) [[unlikely]] {
            allocate_block();
        }

        char* dest = current_;
        std::memcpy(dest, str.data(), str.size());
        current_ += str.size();
        remaining_ -= str.size();

        return {dest, str.size()};
    }

    [[nodiscard]]
    auto size() const noexcept -> size_t
    {
        return (blocks_.size() * BLOCK_SIZE) - remaining_;
    }

    void clear() noexcept
    {
        blocks_.clear();
        current_ = nullptr;
        remaining_ = 0;
    }

  private:
    void allocate_block()
    {
        auto block = std::make_unique<char[]>(BLOCK_SIZE);
        current_ = block.get();
        remaining_ = BLOCK_SIZE;
        blocks_.push_back(std::move(block));
    }
};

} // namespace kumi
