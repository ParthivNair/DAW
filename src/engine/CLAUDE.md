# src/engine — rules

- **GUI-free invariant**: this code compiles into `daw_core` and runs headless under
  `daw_tests`. No `juce_gui_*` includes, no `Component`/`Graphics`/window types. Engine glue only.
- `dsp/` (when created) is under the CI grep tripwire (`tools/rt-tripwire.sh`): no `std::mutex`,
  `malloc`, or bare `new` reachable from the audio callback. RT rules: `docs/realtime-rules.md`,
  cross-thread traffic via `src/rt/` only.
- **Engine docs = the demo sources** (`libs/tracktion_engine/examples/`) + headers + JUCE forum,
  NOT the Doxygen. Never bulk-read engine module internals into context; prefer headers/demos.
- Engine gotchas already paid for (read before touching plugins/render) — `dev/decisions.md`,
  Chunk 4 entry: `ToneGeneratorPlugin` needs `createBuiltInType<>()` first; a plugin `CachedValue`
  needs `updateFromAttachedValue()` to reach the DSP; headless render must pump `RenderTask`
  inline (not `renderToFile`); `Engine`-constructing code needs the `ScopedJuceInitialiser_GUI`
  from `tests/TestMain.cpp`.
