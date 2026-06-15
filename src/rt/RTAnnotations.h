// Real-time function-effect annotations.
//
// We are clang-only on macOS, so the attributes are always live: they drive
// clang's compile-time function-effect analysis (-Wfunction-effects) on EVERY
// build, and RTSan's runtime checks under the `rtsan` preset. The __clang__
// guards exist purely so the headers stay portable to a hypothetical non-clang
// tool (e.g. an IDE indexer) — there they degrade to no-ops.
//
// Placement: these are function-TYPE attributes (like noexcept), so they go
// AFTER the parameter list (and after noexcept), NOT in front of the
// declaration. Clang rejects the leading-attribute form:
//
//     void process (float* buf, int n) noexcept RT_NONBLOCKING;   // correct
//     RT_NONBLOCKING void process (...);                          // ERROR
//
// See docs/realtime-rules.md for the binding spec.
#pragma once

#if defined(__clang__)
 // Functions reachable from the audio callback. Marks a "nonblocking" effect:
 // the analysis flags any allocation, lock, syscall, or call to a non-RT-safe
 // function within, and RTSan aborts at runtime if one happens anyway.
 // Place AFTER the parameter list / noexcept (it is a function-type attribute).
 #define RT_NONBLOCKING [[clang::nonblocking]]

 // Marks a function we KNOW is unsafe for the audio thread (allocates, locks,
 // does IO). The effect analysis then catches any RT_NONBLOCKING caller of it.
 // Place AFTER the parameter list / noexcept, like RT_NONBLOCKING.
 #define RT_BLOCKING [[clang::blocking]]
#else
 #define RT_NONBLOCKING
 #define RT_BLOCKING
#endif
