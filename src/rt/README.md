# src/rt

The **single facade** for all cross-thread (audio ↔ UI/message) communication: lock-free
SPSC queue wrappers, RT-safe shared-state objects, the `RT_NONBLOCKING` macro, and the
debug-build allocation guard.

No code outside this directory may introduce a new cross-thread mechanism that the audio
thread touches. See `docs/realtime-rules.md`.
