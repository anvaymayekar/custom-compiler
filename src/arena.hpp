#pragma once
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <utility>

class ArenaAllocator final {
   public:
    explicit ArenaAllocator(size_t bytes)
        : _size{bytes}, _buffer{new std::byte[bytes]}, _offset{_buffer} {
    }
    ArenaAllocator(const ArenaAllocator &) = delete;
    ArenaAllocator &operator=(const ArenaAllocator &) = delete;

    ArenaAllocator(ArenaAllocator &&other) noexcept
        : _size{std::exchange(other._size, 0)},
          _buffer{std::exchange(other._buffer, nullptr)},
          _offset{std::exchange(other._offset, nullptr)} {
    }
    ArenaAllocator &operator=(ArenaAllocator &&other) noexcept {
        std::swap(_size, other._size);
        std::swap(_buffer, other._buffer);
        std::swap(_offset, other._offset);
        return *this;
    }

    template <typename T>
    [[nodiscard]] T *alloc() {
        size_t remainingBytes = _size - static_cast<size_t>(_offset - _buffer);
        auto ptr = static_cast<void *>(_offset);
        const auto alignedAddr =
            std::align(alignof(T), alignof(T), ptr, remainingBytes);
        if (alignedAddr == nullptr) { throw std::bad_alloc{}; }
        _offset = static_cast<std::byte *>(alignedAddr) + sizeof(T);
        return static_cast<T *>(alignedAddr);
    }

    template <typename T, typename... Args>
    [[nodiscard]] T *emplace(Args &&...args) {
        const auto allocatedMemory = alloc<T>();
        return new (allocatedMemory) T{std::forward<Args>(args)...};
    }

    ~ArenaAllocator() {
        delete[] _buffer;
    }

   private:
    size_t _size;
    std::byte *_buffer;
    std::byte *_offset;
};