# tests

Catch2 test suite, built as the console target `daw_tests` (links `daw_core` only — never the
GUI app). Pyramid: buffer-level DSP unit tests → 1–5 s offline render tests
(`Renderer::renderToFile`, headless engine) → a few end-to-end golden renders → host smoke tests.

- `tests/render/` — render-to-WAV tests: tolerance-based comparison by default, null tests for
  refactors, sample-exact goldens only with pinned dependency SHAs.
- `tests/fixtures/` — `.tracktionedit` fixtures and golden WAVs (**Git LFS**). Regenerate goldens
  only via the explicit regen target.

Keep the whole suite under ~1 minute so it can gate every Claude Code turn.
