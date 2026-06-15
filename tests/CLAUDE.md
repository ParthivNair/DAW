# tests — rules

- **Adding a test**: drop the `.cpp` under `tests/` (or `tests/render/`) AND add it to the
  `add_executable(daw_tests ...)` list in the root `CMakeLists.txt` — sources are listed
  explicitly, NOT globbed. `catch_discover_tests` then auto-registers each `TEST_CASE` with ctest;
  no per-test CMake plumbing. `daw_tests` links `daw_core` only (never the GUI app).
- **Tags** (Catch2): `[sanity]` env/build smoke, `[rt]` real-time facade (`[spsc]`, `[guard]`),
  `[render]` render-to-WAV. Run a slice: `build/dev/daw_tests "[render]"`.
- **Render tests**: copy `tests/render/SineRenderTest.cpp` + reuse `tests/render/RenderHelpers.h`
  (build Edit -> `renderEditToWav` -> `readWavToBuffer` -> assert with real tolerances). Default
  to tolerance/metric assertions; sample-exact goldens only with pinned dep SHAs. See the
  `render-test` skill for the gotchas (inline `RenderTask` pump, `ScopedJuceInitialiser`, the
  ToneGenerator quirks).
- **Fixtures & goldens** live in Git LFS under `tests/fixtures/`. Regenerate goldens ONLY via the
  explicit regen target — never overwrite a golden in place.
- Keep the whole suite **< 1 minute**: it gates every Claude turn (the Stop hook builds
  `daw_tests` + runs `ctest --preset dev`).
