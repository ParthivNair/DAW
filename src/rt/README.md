# src/rt

The **single facade** for all cross-thread (audio ↔ UI/message) communication.
No code outside this directory may introduce a new cross-thread mechanism that
the audio thread touches. See `docs/realtime-rules.md` for the binding rules.

Contents:

- `RTAnnotations.h` — `RT_NONBLOCKING` / `RT_BLOCKING` macros (→ `[[clang::nonblocking]]`
  / `[[clang::blocking]]`). Place them **after** the parameter list / `noexcept` —
  they are function-type attributes. They drive clang's compile-time
  `-Wfunction-effects` analysis and RTSan's runtime checks.
- `SpscQueue.h` — lock-free single-producer/single-consumer queue over
  `moodycamel::ReaderWriterQueue`. Pre-sized at construction; hands out a
  `SpscProducer` (`try_push`) and `SpscConsumer` (`try_pop`/`peek`/`pop`) view.
  The allocating `enqueue` / growing path is unreachable from these views by
  design. This is the canonical UI↔audio messaging primitive.
- `RealtimeGuard.h` / `RealtimeGuard.cpp` — debug-build allocation tripwire.
  `ScopedRealtimeContext` marks the calling thread real-time; while marked, the
  replaced global `operator new` (defined once, in `RealtimeGuard.cpp`) aborts on
  any heap allocation. Gated on `DAW_RT_CHECKS` (Debug only; zero overhead in
  Release). `setRealtimeViolationHandler` is a test-only hook to observe a
  violation instead of aborting.
