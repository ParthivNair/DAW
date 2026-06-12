# DAW project (Tracktion Engine + JUCE 8, C++20, macOS)

Personal DAW with integrated online sound discovery (Freesound + CLAP semantic search).
Plan of record: `dev/TODO.md`. Research findings: `Research/` (read `Research/07-deliverables.md` first).
Decisions log: `dev/decisions.md`.

## Build & test

Presets: `dev` (Debug, native arm64), `release` (universal2), `rtsan` (Homebrew LLVM,
`-fsanitize=realtime`), `asan`. Ninja + ccache; **never the Xcode generator**.

- Configure: `cmake --preset dev`
- Build: `cmake --build --preset dev`            (targets: `daw_core`, `daw_tests`, `EZStudio`)
- Test: `ctest --preset dev`
- Single test: `build/dev/daw_tests "[tag]"`     (e.g. `"[sanity]"`; Catch2 tag filter)
- Fresh clone bootstrap: `tools/bootstrap.sh`

`compile_commands.json` lands in `build/dev/` for clangd. Configure symlinks `libs/rubberband`
into the engine's `modules/3rd_party/`; that link is regenerated per build dir, never committed.

## Audio-thread rules (HARD RULES — see docs/realtime-rules.md)

- No allocation, locks, file/network IO, or logging in anything reachable from the audio callback.
- All cross-thread communication goes through the wrappers in `src/rt/`. Never add a `std::mutex`
  that audio code can touch.
- Functions intended for the audio thread are annotated `RT_NONBLOCKING`
  (expands to `[[clang::nonblocking]]`).

## Verification

- DSP/engine changes MUST come with or update a render test in `tests/render/`.
- Run the tests and paste output before claiming done.
- Golden WAV fixtures live in Git LFS under `tests/fixtures/`; regenerate only via the explicit
  regen target, never by overwriting in place.

## Gotchas

- `libs/tracktion_engine` (and the JUCE inside it) are pinned submodules — never edit them.
  Local patches go in `patches/` with documentation.
- Build against the engine's own JUCE submodule, only that version.
- The engine's real documentation is its demo source (`examples/DemoRunner/demos/`) and the
  JUCE forum Tracktion category — the Doxygen is thin. Don't read engine module internals into
  context unprompted; prefer headers and the demos.
- Project license: GPL-3.0-or-later; builds incorporate JUCE under AGPL-3.0. New dependencies
  must be GPLv3-compatible and recorded in `THIRD_PARTY_LICENSES.md`.
