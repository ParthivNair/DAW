# tests

Catch2 test suite, built as the console target `daw_tests` (links `daw_core` only — never the
GUI app). Pyramid: buffer-level DSP unit tests → 1–5 s offline render tests
(`Renderer::renderToFile`, headless engine) → a few end-to-end golden renders → host smoke tests.

- `tests/render/` — render-to-WAV tests: tolerance-based comparison by default, null tests for
  refactors, sample-exact goldens only with pinned dependency SHAs.
  - `RenderHelpers.h` is the reusable spine: `renderEditToWav` (headless, synchronous — it pumps
    a `Renderer::RenderTask` inline rather than going through `renderToFile`'s progress-bar UI
    path, which never returns without a message loop), `readWavToBuffer`, plus `rmsDbfs`,
    `dominantFrequency` (windowed `juce::dsp::FFT`), and `hasNonFinite`. New render tests build an
    Edit, call these, then assert with real numeric tolerances.
  - `SineRenderTest.cpp` (tag `[render]`) is the template: a 440 Hz `ToneGeneratorPlugin` sine at
    level 0.5 → -9.03 dBFS RMS, dominant bin == 440 Hz within FFT bin resolution, finite + not
    silent. melatonin sparklines/matchers make the failure output readable.
- `tests/fixtures/` — `.tracktionedit` fixtures and golden WAVs (**Git LFS**). Regenerate goldens
  only via the explicit regen target.
- `tests/TestMain.cpp` — a Catch2 listener that holds one `juce::ScopedJuceInitialiser_GUI` for
  the whole run. Required by any test that constructs a tracktion `Engine`, otherwise JUCE's
  global singletons read as "leaked" at process exit. (`Catch2WithMain` still owns `main()`.)

Keep the whole suite under ~1 minute so it can gate every Claude Code turn.
