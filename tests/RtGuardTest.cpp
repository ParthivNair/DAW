// Tests for the rt::RealtimeGuard debug-build allocation tripwire.
//
// We can't let the production handler (abort()) run inside the test process, so
// these tests install the test-only injectable handler to OBSERVE a violation
// instead. The handler records that it fired (and the size) and then returns,
// letting the allocation proceed so the test can clean up normally.
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstddef>
#include <vector>

#include "rt/RealtimeGuard.h"

#if DAW_RT_CHECKS

namespace
{
    std::atomic<int> g_violationCount { 0 };
    std::atomic<std::size_t> g_lastBytes { 0 };

    void recordingHandler (std::size_t bytes) noexcept
    {
        g_violationCount.fetch_add (1, std::memory_order_relaxed);
        g_lastBytes.store (bytes, std::memory_order_relaxed);
        // Deliberately return (do NOT abort) so the test runner survives.
    }

    // RAII: swap in the recording handler for the test, restore the previous
    // (production) handler on exit.
    struct ScopedTestHandler
    {
        ScopedTestHandler() noexcept
        {
            g_violationCount.store (0, std::memory_order_relaxed);
            g_lastBytes.store (0, std::memory_order_relaxed);
            previous = daw::rt::setRealtimeViolationHandler (&recordingHandler);
        }
        ~ScopedTestHandler() noexcept { daw::rt::setRealtimeViolationHandler (previous); }

        daw::rt::RealtimeViolationHandler previous { nullptr };
    };
}

TEST_CASE ("RealtimeGuard: allocation inside a scoped RT context is detected", "[rt][guard]")
{
    ScopedTestHandler swap;

    {
        daw::rt::ScopedRealtimeContext rt;
        REQUIRE (daw::rt::isCurrentThreadRealtime());

        // This heap allocation must trip the guard.
        volatile auto* leak = new int (42);
        delete const_cast<int*> (leak);
    }

    REQUIRE (g_violationCount.load (std::memory_order_relaxed) >= 1);
    REQUIRE (g_lastBytes.load (std::memory_order_relaxed) >= sizeof (int));
}

TEST_CASE ("RealtimeGuard: allocation OUTSIDE an RT context is allowed", "[rt][guard]")
{
    ScopedTestHandler swap;

    REQUIRE_FALSE (daw::rt::isCurrentThreadRealtime());

    std::vector<int> v;
    v.reserve (256); // heap allocation, but we're not in an RT context

    REQUIRE (g_violationCount.load (std::memory_order_relaxed) == 0);
}

TEST_CASE ("RealtimeGuard: the context flag is scoped and nests", "[rt][guard]")
{
    REQUIRE_FALSE (daw::rt::isCurrentThreadRealtime());
    {
        daw::rt::ScopedRealtimeContext outer;
        REQUIRE (daw::rt::isCurrentThreadRealtime());
        {
            daw::rt::ScopedRealtimeContext inner;
            REQUIRE (daw::rt::isCurrentThreadRealtime());
        }
        // still inside outer
        REQUIRE (daw::rt::isCurrentThreadRealtime());
    }
    REQUIRE_FALSE (daw::rt::isCurrentThreadRealtime());
}

#else // DAW_RT_CHECKS disabled

TEST_CASE ("RealtimeGuard: compiled out in release", "[rt][guard]")
{
    daw::rt::ScopedRealtimeContext rt; // no-op
    REQUIRE_FALSE (daw::rt::isCurrentThreadRealtime());
}

#endif // DAW_RT_CHECKS
