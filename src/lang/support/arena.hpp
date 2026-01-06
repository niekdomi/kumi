#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace kumi::lang {

class Arena final
{
    std::vector<std::vector<std::byte>> blocks_;
    std::span<std::byte> current_block_;

    static constexpr std::size_t BLOCK_SIZE = static_cast<const std::size_t>(64 * 1'024);

  public:
    constexpr Arena() = default;

    Arena(const Arena&) = delete;
    auto operator=(const Arena&) -> Arena& = delete;
    Arena(Arena&&) noexcept = default;
    auto operator=(Arena&&) noexcept -> Arena& = default;

    [[nodiscard]]
    auto store(std::string_view str) -> std::string_view
    {
        if (str.empty()) [[unlikely]] {
            return {};
        }

        if (str.size() > current_block_.size()) [[unlikely]] {
            allocate_block(str.size());
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        char* const dest_ptr = reinterpret_cast<char*>(current_block_.data());

        std::ranges::copy(str, dest_ptr);

        current_block_ = current_block_.subspan(str.size());

        return {dest_ptr, str.size()};
    }

    /**
     * @brief Creates an object in the arena.
     * Returns a reference to avoid cppcoreguidelines-owning-memory (T* suggests ownership).
     */
    template<typename T, typename... Args>
    [[nodiscard]]
    auto make(Args&&... args) -> T&
    {
        void* ptr = current_block_.data();
        std::size_t space = current_block_.size();

        if (!std::align(alignof(T), sizeof(T), ptr, space)) [[unlikely]] {
            allocate_block(sizeof(T) + alignof(T));
            ptr = current_block_.data();
            space = current_block_.size();
            std::align(alignof(T), sizeof(T), ptr, space);
        }

        // Advance current_block_ to the new aligned position + size
        const std::size_t offset = current_block_.size() - space + sizeof(T);
        current_block_ = current_block_.subspan(offset);

        // Construct and return reference (semantics: Arena owns it, you use it)
        return *new (ptr) T(std::forward<Args>(args)...);
    }

    void clear() noexcept
    {
        blocks_.clear();
        current_block_ = {};
    }

  private:
    void allocate_block(std::size_t min_required)
    {
        const std::size_t size = std::max(BLOCK_SIZE, min_required);
        auto& block = blocks_.emplace_back();
        block.resize(size);
        current_block_ = std::span{block};
    }
};

} // namespace kumi::lang
