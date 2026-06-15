// The ONE translation unit of daw_core that replaces the global operator
// new/delete to implement the debug-build allocation tripwire. See
// RealtimeGuard.h for the contract.
//
// When DAW_RT_CHECKS is off (release) this file defines nothing — the global
// operators are left untouched and there is zero overhead.
#include "rt/RealtimeGuard.h"

#if DAW_RT_CHECKS

 #include <atomic>
 #include <cstdio>
 #include <cstdlib>
 #include <new>
 #include <unistd.h>

namespace daw::rt
{
namespace
{
    // One marker per thread; allocations only trip the guard on threads that
    // have entered a ScopedRealtimeContext. Nesting depth lets contexts nest.
    thread_local int g_realtimeDepth = 0;

    void defaultViolationHandler (std::size_t bytes) noexcept
    {
        // The failure path must NOT allocate: fixed buffer + raw write()/fputs.
        // (No std::string, no iostreams, no printf with %zu through std::format.)
        char msg[160];
        // snprintf into a fixed stack buffer does not heap-allocate.
        const int n = std::snprintf (msg, sizeof (msg),
                                     "[rt::RealtimeGuard] FATAL: heap allocation of %zu bytes "
                                     "on a thread inside a real-time context. Aborting.\n",
                                     bytes);
        if (n > 0)
            ::write (STDERR_FILENO, msg, static_cast<size_t> (n) < sizeof (msg) ? static_cast<size_t> (n) : sizeof (msg));
        else
            std::fputs ("[rt::RealtimeGuard] FATAL: RT allocation. Aborting.\n", stderr);

        std::abort();
    }

    // Stored as a plain atomic pointer; set rarely (test setup), read on the
    // alloc path. relaxed is fine — there is no other state to order against.
    std::atomic<RealtimeViolationHandler> g_handler { &defaultViolationHandler };
} // namespace

bool isCurrentThreadRealtime() noexcept
{
    return g_realtimeDepth > 0;
}

RealtimeViolationHandler setRealtimeViolationHandler (RealtimeViolationHandler handler) noexcept
{
    if (handler == nullptr)
        handler = &defaultViolationHandler;
    return g_handler.exchange (handler, std::memory_order_relaxed);
}

ScopedRealtimeContext::ScopedRealtimeContext() noexcept { ++g_realtimeDepth; }
ScopedRealtimeContext::~ScopedRealtimeContext() noexcept { --g_realtimeDepth; }

namespace
{
    // The single choke point. Invoked from every replaced operator new before it
    // forwards to malloc; fires the handler (abort by default) if we're inside an
    // RT context. Returns normally otherwise.
    inline void checkRealtimeAllocation (std::size_t bytes) noexcept
    {
        if (g_realtimeDepth > 0)
            g_handler.load (std::memory_order_relaxed) (bytes);
    }
} // namespace
} // namespace daw::rt

// ----------------------------------------------------------------------------
//  Replaced global operators. We forward to std::malloc/std::free so behaviour
//  is otherwise identical to the default implementation. All variants — throwing,
//  nothrow, array, and aligned (C++17) — funnel through the same check so the
//  guard cannot be sidestepped by a particular allocation form.
// ----------------------------------------------------------------------------
namespace
{
void* rtGuardedAlloc (std::size_t size) // may throw std::bad_alloc
{
    daw::rt::checkRealtimeAllocation (size);
    if (size == 0)
        size = 1; // never return nullptr for a 0-byte request
    void* p = std::malloc (size);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

void* rtGuardedAllocNoThrow (std::size_t size) noexcept
{
    daw::rt::checkRealtimeAllocation (size);
    if (size == 0)
        size = 1;
    return std::malloc (size);
}

void* rtGuardedAllocAligned (std::size_t size, std::align_val_t al) // may throw
{
    daw::rt::checkRealtimeAllocation (size);
    const auto alignment = static_cast<std::size_t> (al);
    if (size == 0)
        size = alignment; // keep size a positive multiple of alignment
    // C++17: requested size must be a multiple of alignment for aligned_alloc.
    if (const auto rem = size % alignment; rem != 0)
        size += alignment - rem;
    void* p = std::aligned_alloc (alignment, size);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

void* rtGuardedAllocAlignedNoThrow (std::size_t size, std::align_val_t al) noexcept
{
    daw::rt::checkRealtimeAllocation (size);
    const auto alignment = static_cast<std::size_t> (al);
    if (size == 0)
        size = alignment;
    if (const auto rem = size % alignment; rem != 0)
        size += alignment - rem;
    return std::aligned_alloc (alignment, size);
}
} // namespace

void* operator new (std::size_t size) { return rtGuardedAlloc (size); }
void* operator new[] (std::size_t size) { return rtGuardedAlloc (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept { return rtGuardedAllocNoThrow (size); }
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept { return rtGuardedAllocNoThrow (size); }

void* operator new (std::size_t size, std::align_val_t al) { return rtGuardedAllocAligned (size, al); }
void* operator new[] (std::size_t size, std::align_val_t al) { return rtGuardedAllocAligned (size, al); }
void* operator new (std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept { return rtGuardedAllocAlignedNoThrow (size, al); }
void* operator new[] (std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept { return rtGuardedAllocAlignedNoThrow (size, al); }

// Deallocation is not guarded for RT violations here (free on the audio thread
// is also forbidden, but RTSan covers free; replacing delete only to abort would
// risk aborting during stack unwinding). We just forward to free so new/delete
// stay paired against the same allocator (std::malloc above).
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
void operator delete (void* p, std::align_val_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { std::free (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { std::free (p); }
void operator delete (void* p, std::align_val_t, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete[] (void* p, std::align_val_t, const std::nothrow_t&) noexcept { std::free (p); }

#endif // DAW_RT_CHECKS
