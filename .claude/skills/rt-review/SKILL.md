---
name: rt-review
description: >-
  Real-time-safety review of a diff that touches audio-path code (anything
  reachable from processBlock / the playback graph / src/engine/dsp/, or marked
  RT_NONBLOCKING). Use before finishing such a change, or when asked to review a
  diff for audio-thread safety. Walks the forbidden-operations checklist from
  docs/realtime-rules.md and reports violations as file:line, plus the repo's
  enforcement points.
---

# Real-time-safety diff review

An allocation/lock/IO on the audio thread passes every unit test and renders bit-identical WAVs,
then glitches live. This review is the cheap, high-recall first pass. Source of truth:
`docs/realtime-rules.md`.

## Scope: what counts as audio-path

Anything reachable from the audio callback — `processBlock`, engine graph nodes, anything the
playback graph calls, and any function annotated `RT_NONBLOCKING`. `src/engine/dsp/` is audio-path
by definition. When unsure whether a call is reachable from the RT thread, treat it as if it is.

## Checklist — for each hunk, flag `file:line`

1. **Heap alloc/free** — `new` / `delete` / `malloc` / `free`; `std::vector`/`std::string` growth or
   construction; `std::function` that may allocate; container `.resize()`/`.push_back()` past
   capacity; `std::make_shared`/`make_unique` on the RT path.
2. **`shared_ptr` copies** — copying a `shared_ptr` (atomic refcount bump + possible free on the last
   release). Pass by `const&` / raw pointer / pre-owned handle instead.
3. **Locks** — `std::mutex`, `juce::CriticalSection`, `ScopedLock`, anything that can block or
   priority-invert.
4. **File / network IO** — including logging (`juce::Logger`, `printf`, `std::cout`, `DBG`).
5. **Unbounded waits / syscalls** — condition-variable waits, `thread::join`, sleeps, blocking
   syscalls.
6. **Unbounded loops** — loop bounds that depend on non-RT state (queue drains, container sizes that
   can grow elsewhere).
7. **Hidden offenders** — throwing/`try`-`catch` on the RT path; `std::format`/`ostream`; lazy
   statics with a guard lock; virtual calls into non-RT-safe implementations.

For each finding: `path:line — what — why it's RT-unsafe — the fix` (usually: pre-size in a
`prepareToPlay`-equivalent, or route via `src/rt/`).

## Required patterns (the fix usually is one of these)

- **Cross-thread traffic goes through `src/rt/` only** — `SpscQueue` (`try_push`/`try_pop`, never
  the allocating `enqueue`/growing path), RT-safe shared-state wrappers. Don't improvise a new
  mechanism at the call site; add it to `src/rt/`.
- **Pre-size everything** in setup; the RT path only reads/writes pre-allocated buffers.
- **Annotate entry points** `RT_NONBLOCKING` (→ `[[clang::nonblocking]]`); mark known-unsafe own
  functions `RT_BLOCKING` so the effect analysis catches accidental calls. These are
  **function-type attributes — place them AFTER the parameter list / `noexcept`**, e.g.
  `bool try_push (T&&) noexcept RT_NONBLOCKING`. Misplaced, they silently do nothing.

## Repo enforcement points (where this is also caught)

- **Compile-time**: `RT_NONBLOCKING`/`RT_BLOCKING` drive clang's `-Wfunction-effects` on every
  build (`src/rt/RTAnnotations.h`).
- **Runtime**: the `rtsan` preset (Homebrew LLVM clang, `-fsanitize=realtime`) — render tests are
  the RTSan workload. Also the Debug allocation tripwire (`src/rt/RealtimeGuard.*`,
  `ScopedRealtimeContext`, gated on `DAW_RT_CHECKS`).
- **Grep tripwire**: `tools/rt-tripwire.sh` scans `src/engine/dsp/` for
  `std::mutex|malloc|\bnew\b` (CI + run locally). A finding here is a hard fail.

Verify a touched audio path with `ctest --preset rtsan` (and `tsan` for races) before done.
