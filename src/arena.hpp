#pragma once
#include <cstddef>
#include <cstdlib>
class ArenaAllocator {
   public:
    inline ArenaAllocator(size_t bytes) : _size(bytes) {
        _buffer = static_cast<std::byte *>(malloc(_size));
        _offset = _buffer;
    }
    template <typename T>
    inline T *alloc() {
        void *offset = _offset;
        _offset += sizeof(T);
        return static_cast<T *>(offset);
    }
    inline ArenaAllocator(const ArenaAllocator &other) = delete;
    inline ArenaAllocator operator=(const ArenaAllocator &other) = delete;
    inline ~ArenaAllocator() {
        free(_buffer);
    }

   private:
    size_t _size;
    std::byte *_buffer;
    std::byte *_offset;
};