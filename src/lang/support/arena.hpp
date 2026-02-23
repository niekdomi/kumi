/// @file arena.hpp
/// @brief Memory arena allocator for fast allocations

#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace kumi::lang {

/// @brief Memory arena allocator for fast, bump-pointer style allocation
///
/// Allocates memory in large blocks and sub-allocates from them. Objects are
/// never individually freed; call `clear()` to reset. Does not call destructors -
/// only suitable for trivially destructible types or when manual lifetime
/// management is acceptable.
class Arena final
{
  public:
    Arena(const Arena&) = delete;
    auto operator=(const Arena&) -> Arena& = delete;
    Arena(Arena&&) noexcept = default;
    auto operator=(Arena&&) noexcept -> Arena& = default;
    ~Arena() = default;

    explicit Arena()
    {
        allocate_block(BLOCK_SIZE);
    }

    /// @brief Clears all allocated memory in the arena
    auto clear() noexcept -> void
    {
        blocks_.clear();
        current_block_ = {};
    }

    /// @brief Construct an object of type T in the arena
    /// @tparam T Type to construct
    /// @tparam Args Constructor argument types
    /// @param args Arguments forwarded to T's constructor
    /// @return Reference to the constructed object
    template<typename T, typename... Args>
    [[nodiscard]]
    auto make(Args&&... args) -> T&
    {
        void* ptr = allocate_bytes(sizeof(T), alignof(T));
        return *std::construct_at(static_cast<T*>(ptr), std::forward<Args>(args)...);
    }

  private:
    /// @brief Storage for all allocated memory blocks
    std::vector<std::vector<std::byte>> blocks_;
    /// @brief View of remaining unallocated bytes in the current block
    std::span<std::byte> current_block_;
    /// @brief Default size for new blocks
    static constexpr std::size_t BLOCK_SIZE = 4'096;

    // TODO(domi): Find an appropriate default for BLOCK_SIZE or make this size
    // generic based on input somehow or e.g., first block is 4KB, then 8KB, ...

    /// @brief Allocate a new memory block and make it current
    /// @param min_size Minimum size required; actual size is max(BLOCK_SIZE, min_size)
    auto allocate_block(std::size_t min_size) -> void
    {
        const std::size_t block_size = std::max(BLOCK_SIZE, min_size);
        auto& block = blocks_.emplace_back(block_size);
        current_block_ = block;
    }

    /// @brief Allocate raw memory with specified size and alignment
    /// @param size Number of bytes to allocate
    /// @param alignment Required alignment in bytes (must be power of 2)
    /// @return Pointer to aligned memory
    [[nodiscard]]
    auto allocate_bytes(std::size_t size, std::size_t alignment) -> void*
    {
        // NOLINTNEXTLINE(misc-const-correctness) -> false positive
        void* ptr = current_block_.data();
        std::size_t space = current_block_.size();

        if (std::align(alignment, size, ptr, space) == nullptr) [[unlikely]] {
            allocate_block(size + alignment);
            ptr = current_block_.data();
            space = current_block_.size();
            std::align(alignment, size, ptr, space);
        }

        const std::size_t offset = current_block_.size() - space + size;
        current_block_ = current_block_.subspan(offset);

        return ptr;
    }
};

} // namespace kumi::lang

