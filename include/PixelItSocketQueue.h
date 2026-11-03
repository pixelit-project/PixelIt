#pragma once
#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <new>
#include <cstring>

// ESP32 only: owned, bounded bytes survive the caller's temporary JSON string.
// Retain one allocation until disconnect; never allocate for individual frames.
class SocketOutputQueue
{
public:
    static constexpr size_t capacity = 4096;
    bool append(const uint8_t *data, size_t length)
    {
        if (length > capacity - used) return false;
        if (!storage) storage.reset(new (std::nothrow) uint8_t[capacity]);
        if (!storage) return false;
        const size_t tail = (head + used) % capacity;
        const size_t first = length < capacity - tail ? length : capacity - tail;
        memcpy(storage.get() + tail, data, first);
        memcpy(storage.get(), data + first, length - first);
        used += length;
        return true;
    }
    const uint8_t *data() const { return storage.get() + head; }
    size_t contiguous() const { return used < capacity - head ? used : capacity - head; }
    size_t size() const { return used; }
    void consume(size_t length) { head = (head + length) % capacity; used -= length; }
    void reset() { storage.reset(); head = used = 0; fd = -1; progress = 0; enabled = false; }
    bool enabled = false;
    int fd = -1;
    unsigned long progress = 0;
private:
    std::unique_ptr<uint8_t[]> storage;
    size_t head = 0, used = 0;
};
