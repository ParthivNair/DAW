# Real-time audio-thread rules

These are hard rules for any code reachable from the audio callback (`processBlock`, engine
graph nodes, anything the playback graph calls). A violation passes every unit test and renders
bit-identical WAVs — then glitches live. Enforcement is layered: convention (this file +
CLAUDE.md), compile-time (`-Wfunction-effects`), runtime (RTSan lane), and grep tripwires in CI.

## Forbidden on the audio thread

1. **Heap allocation / deallocation** — no `new`/`delete`/`malloc`, no `std::vector` growth,
   no `std::string` construction, no `std::function` that may allocate, no `shared_ptr` copies
   (atomic refcount + potential free).
2. **Locks** — no `std::mutex`, `CriticalSection`, or anything that can block or priority-invert.
3. **File / network IO** — none, including logging.
4. **Syscalls with unbounded latency** — no waiting on condition variables, no thread joins.
5. **Unbounded loops** whose iteration count depends on non-RT state.

## Required patterns

- **All cross-thread communication goes through `src/rt/`** — the single facade. Lock-free SPSC
  queues (`try_enqueue` on the RT side, never the allocating `enqueue`), RT-safe shared-state
  wrappers. If a new communication pattern is needed, add it to `src/rt/`, don't improvise at
  the call site.
- **Annotate RT entry points** with `RT_NONBLOCKING` (expands to `[[clang::nonblocking]]` —
  enables clang's compile-time function-effect analysis on every build, and RTSan's runtime
  checks in the `rtsan` preset).
- Mark known-unsafe own functions `[[clang::blocking]]` so the analysis catches accidental calls.
- Pre-size all buffers in `prepareToPlay`-equivalents; the RT path only reads/writes.

## Verification lanes

- `rtsan` CMake preset (Homebrew LLVM clang ≥ 20, `-fsanitize=realtime`): render tests double
  as the RTSan workload. Runs in CI on every push.
- CI grep tripwire over `src/engine/dsp/`: `rg "std::mutex|malloc|\bnew\b"`.
- Review checklist for any diff touching the audio path: allocations, locks, IO, logging,
  `shared_ptr` copies, unbounded loops — report `file:line`.
