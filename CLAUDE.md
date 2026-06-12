# DAW project (Tracktion Engine + JUCE 8, C++20, macOS)

Personal DAW with integrated online sound discovery (Freesound + CLAP semantic search).
Plan of record: `dev/TODO.md`. Research findings: `Research/` (read `Research/07-deliverables.md` first).
Decisions log: `dev/decisions.md`.

## Build & test

The build system does not exist yet — it is Phase 0 in `dev/TODO.md`. Once it lands, this section
must contain the exact canonical commands (CMake presets: `dev`, `release`, `rtsan`, `asan`;
Ninja + ccache; never the Xcode generator). Update this file in the same PR that adds the build.

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
