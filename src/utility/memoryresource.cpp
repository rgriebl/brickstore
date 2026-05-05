// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "memoryresource.h"


#if defined(__cpp_lib_memory_resource) && !defined(BS_NO_STD_PMR_AVAILABLE)

MemoryResource *defaultMemoryResource()
{
    return std::pmr::get_default_resource();
}

#else

MemoryResource::~MemoryResource()
{ }


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


MemoryResource *defaultMemoryResource()
{
    static NewDeleteMemoryResource newDeleteMR;
    return &newDeleteMR;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

// Member functions for std::pmr::MonotonicMemoryResource

// This was defined inline in 9.1 and 9.2 so code compiled by those
// versions will not use this symbol.
MonotonicMemoryResource::~MonotonicMemoryResource() { release(); }

void MonotonicMemoryResource::new_buffer(size_t bytes, size_t alignment)
{
    const size_t n = std::max(bytes, m_next_bufsiz);
    const size_t m = aligned_ceil(alignment, alignof(std::max_align_t));
    auto [p, size] = _Chunk::allocate(m_upstream, n, m, m_head);
    m_current_buf = p;
    m_avail = size;
    m_next_bufsiz *= s_growth_factor;
}

void MonotonicMemoryResource::release_buffers() noexcept
{
    _Chunk::release(m_head, m_upstream);
}

#endif

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


bool DatabaseMonotonicMemoryResource::debug = false;

DatabaseMonotonicMemoryResource::DatabaseMonotonicMemoryResource(size_t initialSize)
    : MonotonicMemoryResource(initialSize)
    , stepSize(initialSize)
{ }

DatabaseMonotonicMemoryResource::~DatabaseMonotonicMemoryResource() noexcept
{
    if (debug)
        qWarning() << "DB_MBR:" << this << "releasing" << (count / stepSize) << "MB";
}

void *DatabaseMonotonicMemoryResource::do_allocate(const size_t size, const size_t align)
{
    if (debug) {
        if ((count / stepSize) != ((count + size) / stepSize))
            qWarning() << "DB_MBR:" << this << "allocation now over" << ((count + size) / stepSize) << "MB";
        count += size;
    }
    return MonotonicMemoryResource::do_allocate(size, align);
}
void DatabaseMonotonicMemoryResource::do_deallocate(void *ptr, size_t size, size_t align)
{
    if (debug)
        qWarning() << "DB_MBR:" << this << "is requested to deallocate" << size << "bytes";
    MonotonicMemoryResource::do_deallocate(ptr, size, align);
}
