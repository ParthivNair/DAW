---
name: render-test
description: >-
  Author or debug an offline render-to-WAV test for the EZStudio engine (anything
  under tests/render/). Use when adding a DSP/engine test, asserting on rendered
  audio (RMS/level, dominant frequency/FFT, null/difference, finite+non-silent),
  or hitting the headless-render gotchas (RenderTask hangs, silent/wrong-level
  output, JUCE "leaked singleton" at exit). Covers the RenderHelpers pattern,
  assertion tiers, ToneGenerator quirks, and the golden-fixture policy.
---

# Authoring a render test

Render tests are how we verify audio Claude can't hear: build an `Edit`, render it
offline to a WAV, assert on numbers. Template = `tests/render/SineRenderTest.cpp`;
shared spine = `tests/render/RenderHelpers.h`. Read both before writing a new one.

## The shape (copy this)

```
build an Edit  ->  renderEditToWav  ->  readWavToBuffer  ->  assert numerically
```

1. `te::Engine engine { "EZStudio-<name>-test" };` — headless; no audio device is opened.
2. Build the `Edit` programmatically (see the `tracktion-api` skill) or load a `.tracktionedit`
   fixture from `tests/fixtures/`.
3. `daw::render_test::renderEditToWav (edit, lengthSecs, sampleRate)` → a self-deleting
   `juce::TemporaryFile`.
4. `daw::render_test::readWavToBuffer (file, sampleRateOut)` → `std::optional<AudioBuffer<float>>`.
5. Assert. Add the test file to `add_executable(daw_tests ...)` in the root `CMakeLists.txt`
   (sources are listed, not globbed) and tag it `[render]`.

## Assertion tiers (default → strict)

- **Tolerance / metric (default workhorse)** — `rmsDbfs(buffer, ch, edgeSkip)` for level,
  `dominantFrequency(buffer, ch, sr)` for the peak FFT bin (windowed `juce::dsp::FFT`; assert
  within `peak.binResolutionHz`). Skip ~5 ms at each edge so transport ramps don't bias the level.
- **Finite + non-silent** — `hasNonFinite(buffer)` must be false; melatonin matchers
  `isValidAudio()` (no NaN/Inf) and `isFilled()` (not silent). melatonin `sparkline()` in a `WARN`
  makes failures readable.
- **Null / difference test (for refactors)** — render the reference path and the new path,
  subtract, assert residual ≈ silence. Requires exact sample alignment.
- **Sample-exact golden** — `memcmp` vs a committed reference WAV. Brittle; ONLY with pinned
  dependency SHAs. Last resort.

## Gotchas (already paid for — see dev/decisions.md Chunk 4)

- **RenderTask, not renderToFile.** `Renderer::renderToFile(taskDescription, params)` routes
  through `UIBehaviour::runTaskWithProgressBar`, which never returns under a headless engine with
  no message loop (it hangs). `renderEditToWav` instead pumps a `Renderer::RenderTask` inline:
  `while (task.runJob() == jobNeedsRunningAgain) {}`. Reuse the helper; don't reintroduce
  `renderToFile`.
- **ScopedJuceInitialiser.** Any test that constructs an `Engine` needs the process-wide
  `juce::ScopedJuceInitialiser_GUI` held by the Catch2 listener in `tests/TestMain.cpp`, else JUCE
  global singletons read as leaked at exit. It is already wired — just don't remove it.
- **ToneGeneratorPlugin is not a default built-in.** `PluginManager::initialise()` omits it;
  `createNewPlugin("toneGenerator", {})` returns null until you call
  `engine.getPluginManager().createBuiltInType<te::ToneGeneratorPlugin>()` first (idempotent).
- **CachedValue ≠ live parameter.** Setting `tone->frequency = …` (a `CachedValue`) does NOT update
  the `AutomatableParameter` the DSP reads in `applyToBuffer()`. Call `updateFromAttachedValue()`
  on each parameter (mirrors `restorePluginStateFromValueTree`), or you render the plugin defaults
  (220 Hz, level 1.0). `daw::buildSineToneEdit` does this for you.
- **Determinism.** Prefer sources with no live input and no time-stretch nondeterminism. The sine
  tone is bit-stable; auto-tempo / time-stretch paths are not (and need Rubber Band, not
  SoundTouch, for sample-aligned null tests — Chunk 4).

## Golden-fixture policy

Goldens and `.tracktionedit` fixtures live in **Git LFS** under `tests/fixtures/`. Regenerate
goldens ONLY via the explicit regen target — never overwrite a golden in place (so a diff is a
deliberate, reviewable act). Keep fixtures short; the whole suite must stay < 1 minute.

## Verify

`cmake --build --preset dev --target daw_tests && ctest --preset dev` (or
`build/dev/daw_tests "[render]"`). DSP/engine changes MUST land with a render test. Paste the
output before claiming done.
