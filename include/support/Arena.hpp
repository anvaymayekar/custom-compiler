#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mr {

// Thrown instead of std::bad_alloc so callers can tell an arena exhaustion
// apart from a genuine heap-allocation failure elsewhere in the process.
class ArenaExhausted final : public std::runtime_error {
   public:
    ArenaExhausted() : std::runtime_error("arena allocator: block exhausted") {
    }
};

// A growable bump-pointer arena. AST nodes are allocated here and never
// individually freed; the whole arena is released at once when the Parser
// (or whatever owns it) is destroyed. This keeps node allocation cheap and
// keeps lifetime reasoning simple: "as long as the arena lives, every
// pointer it handed out is valid."
//
// Unlike the original single-block version, this grows by allocating new
// blocks on demand rather than throwing once a fixed budget is exhausted -
// callers no longer need to guess a program's memory needs up front.
class Arena final {
   public:
    explicit Arena(std::size_t blockBytes = 1024 * 1024)
        : _blockBytes(blockBytes) {
        addBlock(_blockBytes);
    }

    Arena(const Arena &) = delete;
    Arena &operator=(const Arena &) = delete;
    Arena(Arena &&) noexcept = default;
    Arena &operator=(Arena &&) noexcept = default;
    ~Arena() = default;

    template <typename T>
    [[nodiscard]] T *allocate() {
        void *mem = allocateRaw(sizeof(T), alignof(T));
        return static_cast<T *>(mem);
    }

    template <typename T, typename... Args>
    [[nodiscard]] T *emplace(Args &&...args) {
        T *mem = allocate<T>();
        return new (mem) T{std::forward<Args>(args)...};
    }

    // Total bytes currently reserved across all blocks (for diagnostics /
    // testing only - not needed for correctness).
    [[nodiscard]] std::size_t reservedBytes() const noexcept {
        std::size_t total = 0;
        for (const auto &b : _blocks) { total += b.size; }
        return total;
    }

   private:
    struct Block {
        std::unique_ptr<std::byte[]> buffer;
        std::size_t size = 0;
        std::size_t used = 0;
    };

    void addBlock(std::size_t minBytes) {
        const std::size_t size = std::max(minBytes, _blockBytes);
        Block block;
        block.buffer = std::make_unique<std::byte[]>(size);
        block.size = size;
        block.used = 0;
        _blocks.push_back(std::move(block));
    }

    void *allocateRaw(std::size_t bytes, std::size_t alignment) {
        Block &current = _blocks.back();
        void *cursor = current.buffer.get() + current.used;
        std::size_t remaining = current.size - current.used;

        void *aligned = std::align(alignment, bytes, cursor, remaining);
        if (aligned == nullptr) {
            // Current block doesn't have room; start a fresh block sized to
            // fit at least this allocation (plus alignment slack).
            addBlock(bytes + alignment);
            return allocateRaw(bytes, alignment);
        }

        current.used = static_cast<std::size_t>(
            static_cast<std::byte *>(aligned) - current.buffer.get() + bytes);
        return aligned;
    }

    std::size_t _blockBytes;
    std::vector<Block> _blocks;
};

}  // namespace mr
