// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDebug>

#if __has_include(<memory_resource>)
#  include <memory_resource>
#endif


#ifdef __cpp_lib_memory_resource

using MemoryResource = std::pmr::memory_resource;
using MonotonicMemoryResource = std::pmr::monotonic_buffer_resource;
MemoryResource *defaultMemoryResource();

#else

// PMR is not available, so we have to implement it ourselves.
// Currently this is libc++ on macOS, iOS and Android.
// Both implementations for pmr::memory_resource and pmr::monotonic_buffer_resource are
// a copy of libstdc++'s version, licensed under the GPL3.

struct MemoryResource
{
    static constexpr size_t s_max_align = alignof(max_align_t);

public:
    MemoryResource() = default;
    MemoryResource(const MemoryResource&) = default;
    virtual ~MemoryResource(); // key function

    MemoryResource& operator=(const MemoryResource&) = default;

    [[nodiscard]] void *allocate(size_t bytes, size_t alignment = s_max_align)
        __attribute__((__returns_nonnull__,__alloc_size__(2),__alloc_align__(3)))
    { return ::operator new(bytes, do_allocate(bytes, alignment)); }

    void deallocate(void* p, size_t bytes, size_t alignment = s_max_align)
        __attribute__((__nonnull__))
    { do_deallocate(p, bytes, alignment); }

    bool is_equal(const MemoryResource& other) const noexcept
    { return do_is_equal(other); }

protected:
    virtual void *do_allocate(size_t bytes, size_t alignment) = 0;
    virtual void do_deallocate(void* p, size_t bytes, size_t alignment) = 0;
    virtual bool do_is_equal(const MemoryResource& other) const noexcept = 0;
};

inline bool operator==(const MemoryResource& a, const MemoryResource& b) noexcept
{ return &a == &b || a.is_equal(b); }


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


struct NewDeleteMemoryResource : public MemoryResource
{
protected:
    void *do_allocate(size_t bytes, size_t alignment) override
    { return ::operator new(bytes, std::align_val_t(alignment)); }

    void do_deallocate(void* p, size_t bytes, size_t alignment) override
    { ::operator delete(p, bytes, std::align_val_t(alignment)); }

    bool do_is_equal(const MemoryResource& other) const noexcept override
    { return &other == this; }
};

MemoryResource *defaultMemoryResource();


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class MonotonicMemoryResource : public MemoryResource
{
public:
    explicit MonotonicMemoryResource(MemoryResource* upstream) noexcept
        : m_upstream(upstream)
    { }

    MonotonicMemoryResource(size_t initial_size, MemoryResource* upstream) noexcept
        : m_next_bufsiz(initial_size)
        , m_upstream(upstream)
    { }

    MonotonicMemoryResource(void* buffer, size_t buffer_size, MemoryResource* upstream) noexcept
        : m_current_buf(buffer), m_avail(buffer_size)
        , m_next_bufsiz(s_next_bufsize(buffer_size))
        , m_upstream(upstream)
        , m_orig_buf(buffer), m_orig_size(buffer_size)
    { }

    MonotonicMemoryResource() noexcept
        : MonotonicMemoryResource(defaultMemoryResource())
    { }

    explicit MonotonicMemoryResource(size_t initial_size) noexcept
        : MonotonicMemoryResource(initial_size, defaultMemoryResource())
    { }

    MonotonicMemoryResource(void* buffer, size_t buffer_size) noexcept
        : MonotonicMemoryResource(buffer, buffer_size, defaultMemoryResource())
    { }

    MonotonicMemoryResource(const MonotonicMemoryResource&) = delete;

    virtual ~MonotonicMemoryResource(); // key function

    MonotonicMemoryResource &operator=(const MonotonicMemoryResource&) = delete;

    void release() noexcept
    {
        if (m_head)
            release_buffers();

        // reset to initial state at contruction:
        if ((m_current_buf = m_orig_buf)) {
            m_avail = m_orig_size;
            m_next_bufsiz = s_next_bufsize(m_orig_size);
        } else {
            m_avail = 0;
            m_next_bufsiz = m_orig_size;
        }
    }

    MemoryResource *upstream_resource() const noexcept
    { return m_upstream; }

protected:
    void *do_allocate(size_t bytes, size_t alignment) override
    {
        if (bytes == 0)
            bytes = 1; // Ensures we don't return the same pointer twice.

        void* p = std::align(alignment, bytes, m_current_buf, m_avail);
        if (!p) {
            new_buffer(bytes, alignment);
            p = m_current_buf;
        }
        m_current_buf = (char*)m_current_buf + bytes;
        m_avail -= bytes;
        return p;
    }

    void do_deallocate(void*, size_t, size_t) override
    { }

    bool do_is_equal(const MemoryResource& other) const noexcept override
    { return this == &other; }

private:
    // Update m_current_buf and m_avail to refer to a new buffer with
    // at least the specified size and alignment, allocated from upstream.
    void new_buffer(size_t bytes, size_t alignment);

    // Deallocate all buffers obtained from upstream.
    void release_buffers() noexcept;

    static size_t s_next_bufsize(size_t buffer_size) noexcept
    {
        if (buffer_size == 0)
            buffer_size = 1;
        return buffer_size * s_growth_factor;
    }

    static constexpr size_t s_init_bufsize = 128 * sizeof(void*);
    static constexpr float s_growth_factor = 1.5;

    void*	m_current_buf = nullptr;
    size_t	m_avail = 0;
    size_t	m_next_bufsiz = s_init_bufsize;

    // Initial values set at construction and reused by release():
    MemoryResource* const	m_upstream;
    void* const			m_orig_buf = nullptr;
    size_t const		m_orig_size = m_next_bufsiz;

    class _Chunk;
    _Chunk* m_head = nullptr;
};


namespace {

// aligned_size<N> stores the size and alignment of a memory allocation.
// The size must be a multiple of N, leaving the low log2(N) bits free
// to store the base-2 logarithm of the alignment.
// For example, allocate(1024, 32) is stored as 1024 + log2(32) = 1029.
template<unsigned N>
struct aligned_size
{
    // N must be a power of two
    static_assert( std::popcount(N) == 1 );

    static constexpr size_t s_align_mask = N - 1;
    static constexpr size_t s_size_mask = ~s_align_mask;

    constexpr
        aligned_size(size_t sz, size_t align) noexcept
        : value(sz | (/*std::bit_width(align)*/std::numeric_limits<size_t>::digits - std::countl_zero(align) - 1u))
    {
        assert(size() == sz); // sz must be a multiple of N
    }

    constexpr size_t
    size() const noexcept
    { return value & s_size_mask; }

    constexpr size_t
    alignment() const noexcept
    { return size_t(1) << (value & s_align_mask); }

    size_t value; // size | log2(alignment)
};

// Round n up to a multiple of alignment, which must be a power of two.
constexpr size_t aligned_ceil(size_t n, size_t alignment)
{
    return (n + alignment - 1) & ~(alignment - 1);
}

} // namespace

// Memory allocated by the upstream resource is managed in a linked list
// of _Chunk objects. A _Chunk object recording the size and alignment of
// the allocated block and a pointer to the previous chunk is placed
// at end of the block.
class MonotonicMemoryResource::_Chunk
{
public:
    // Return the address and size of a block of memory allocated from r,
    // of at least size bytes and aligned to align.
    // Add a new _Chunk to the front of the linked list at head.
    static std::pair<void*, size_t> allocate(MemoryResource* r, size_t size, size_t align, _Chunk*& head)
    {
        const size_t orig_size = size;

        // Add space for the _Chunk object and round up to 64 bytes.
        size = aligned_ceil(size + sizeof(_Chunk), 64);

        // Check for unsigned wraparound
        if (size < orig_size) [[unlikely]] {
            // MonotonicMemoryResource::do_allocate is not allowed to throw.
            // If the required size is too large for size_t then ask the
            // upstream resource for an impossibly large size and alignment.
            size = -1;
            align = ~(size_t(-1) >> 1);
        }

        void* p = r->allocate(size, align);

        // Add a chunk defined by (p, size, align) to linked list head.
        // We know the end of the buffer is suitably-aligned for a _Chunk
        // because the caller ensured align is at least alignof(max_align_t).
        void* const back = (char*)p + size - sizeof(_Chunk);
        head = ::new(back) _Chunk(size, align, head);
        return { p, size - sizeof(_Chunk) };
    }

    // Return every chunk in linked list head to resource r.
    static void release(_Chunk*& head, MemoryResource* r) noexcept
    {
        _Chunk* next = head;
        head = nullptr;
        while (next) {
            _Chunk* ch = next;
            next = ch->m_next;
            size_t size = ch->m_size.size();
            size_t align = ch->m_size.alignment();
            void* start = (char*)(ch + 1) - size;
            r->deallocate(start, size, align);
        }
    }

private:
    _Chunk(size_t size, size_t align, _Chunk* next) noexcept
        : m_size(size, align), m_next(next)
    { }

    aligned_size<64> m_size;
    _Chunk* m_next;
};

#endif

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class DatabaseMonotonicMemoryResource : public MonotonicMemoryResource
{
public:
    explicit DatabaseMonotonicMemoryResource(size_t initialSize);
    ~DatabaseMonotonicMemoryResource() noexcept override;

    static bool debug;

protected:
    void *do_allocate(const size_t size, const size_t align) override;
    void do_deallocate(void *ptr, size_t size, size_t align) override;

private:
    size_t stepSize;
    quint64 count = 0;
};
