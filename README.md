# DAW

A personal DAW for macOS built on **Tracktion Engine + JUCE 8** (C++20, CMake), with integrated
online sound discovery (Freesound search with CLAP-based semantic matching) as the differentiating
feature.

## Status

Pre-code. Research is complete; development follows the phased plan in [dev/TODO.md](dev/TODO.md).

## Repo map

| Path | Purpose |
|---|---|
| `Research/` | Research notes the plan is built on (stack, licensing, sound search, plugins, workflow, landscape) |
| `dev/` | Development plan ([TODO.md](dev/TODO.md)), decisions log, feedback notes |
| `src/engine/` | Tracktion Engine glue — edit/transport/render logic, no GUI includes |
| `src/ui/` | All UI (timeline, mixer, browser, plugin windows) — the engine provides none |
| `src/rt/` | Real-time utilities: the single facade for audio↔UI thread communication |
| `src/search/` | Freesound client, query parsing, embeddings, local sample index |
| `tests/` | Catch2 unit + offline render tests; golden fixtures in Git LFS |
| `libs/` | Third-party code (pinned submodules: tracktion_engine, rubberband, …) |
| `tools/` | Dev scripts, golden-regen tooling, the later Python embedding sidecar |
| `docs/` | Project docs (e.g. [realtime-rules.md](docs/realtime-rules.md)) |

## License

Project code: **GPL-3.0-or-later**. Distributed builds incorporate JUCE 8 under **AGPL-3.0**
(one-way compatible). See `THIRD_PARTY_LICENSES.md` (added in Phase 0) for the full dependency
license table; a summary audit lives in [Research/02-licensing-audit.md](Research/02-licensing-audit.md).
