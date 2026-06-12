// Debug-build allocation tripwire for the audio thread (the "AudioGuard" pattern,
// per Research/05 §4 "Conventions an AI can enforce").
//
// A thread-local flag marks the current thread as "in a real-time context".
// While that flag is set, the replaced global operator new/delete (defined in
// RealtimeGuard.cpp — exactly one TU of daw_core) treats any heap allocation as
// a hard error: it writes a clear message to stderr and aborts.
//
// This complements RTSan: it runs in the ordinary `dev` build with no special
// toolchain, catching allocations RTSan would only catch under the rtsan preset.
//
// Cost model:
//   * Enabled when DAW_RT_CHECKS is defined. CMake defines it for the `dev`
//     build (Debug, NDEBUG unset); release builds leave it undefined, so the
//     RAII marker is an empty inline and the operators are NOT replaced —
//     literally zero overhead.
//   * The check itself is a single thread-local bool read per allocation.
//
// See docs/realtime-rules.md.
#pragma once

// Default policy: on in debug (NDEBUG unset) unless the build overrides it.
#if !defined(DAW_RT_CHECKS)
 #if !defined(NDEBUG)
  #define DAW_RT_CHECKS 1
 #else
  #define DAW_RT_CHECKS 0
 #endif
#endif

#include <cstddef>

#include "rt/RTAnnotations.h"

namespace daw::rt
{
#if DAW_RT_CHECKS

/** True iff the calling thread is currently inside a real-time context (i.e. an
    enclosing ScopedRealtimeContext is live). Read by the replaced operator new. */
bool isCurrentThreadRealtime() noexcept;

/** Signature of the handler invoked when an allocation is detected inside a
    real-time context. `bytes` is the requested size. */
using RealtimeViolationHandler = void (*) (std::size_t bytes) noexcept;

/** Installs a violation handler and returns the previous one. The PRODUCTION
    DEFAULT is to write a message to stderr and abort(); this hook exists so
    tests can OBSERVE a violation without killing the runner. Test-only — do not
    use to silence violations in shipping code. Not thread-safe; set it before
    spawning RT threads. */
RealtimeViolationHandler setRealtimeViolationHandler (RealtimeViolationHandler handler) noexcept;

/** RAII marker: sets the calling thread's real-time flag for its lifetime
    (nesting-safe). Construct one at the top of any scope that stands in for the
    audio callback; any allocation inside trips the guard. */
class ScopedRealtimeContext
{
public:
    ScopedRealtimeContext() noexcept;
    ~ScopedRealtimeContext() noexcept;

    ScopedRealtimeContext (const ScopedRealtimeContext&) = delete;
    ScopedRealtimeContext& operator= (const ScopedRealtimeContext&) = delete;
};

#else // DAW_RT_CHECKS disabled — compiled out to nothing.

inline bool isCurrentThreadRealtime() noexcept { return false; }

using RealtimeViolationHandler = void (*) (std::size_t bytes) noexcept;
inline RealtimeViolationHandler setRealtimeViolationHandler (RealtimeViolationHandler) noexcept { return nullptr; }

class ScopedRealtimeContext
{
public:
    ScopedRealtimeContext() noexcept = default;
    ScopedRealtimeContext (const ScopedRealtimeContext&) = delete;
    ScopedRealtimeContext& operator= (const ScopedRealtimeContext&) = delete;
};

#endif // DAW_RT_CHECKS
} // namespace daw::rt
