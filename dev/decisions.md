# Decisions log

Append-only record of project decisions and pinned facts. One dated entry per decision.
Spike verdicts (see `dev/TODO.md` Part B) land here too.

## 2026-06-11 — Stack & license (from research synthesis)

- **Stack**: Tracktion Engine (`develop`, pinned SHA — record here at Phase 0) + its JUCE 8
  submodule; C++20; CMake + Ninja + ccache; VST3 + AU hosting (CLAP punted); CoreAudio I/O;
  Rubber Band time-stretch (SoundTouch fallback); Freesound API + LAION-CLAP
  (`laion/clap-htsat-fused`) for sound search; ONNX Runtime for the text encoder;
  SQLite + sqlite-vec for the local sample index.
- **License**: project code GPL-3.0-or-later; distributed builds incorporate JUCE under
  AGPL-3.0. No dependency blocks this (full audit: `Research/02-licensing-audit.md`).

## Pending (fill in when decided)

- App name / bundle identifier: _TBD_
- Tracktion Engine pinned SHA + JUCE submodule SHA: _TBD (Spike 2)_
- Semantic-search path GO/NO-GO (text-vector `similar_to` trick): _TBD (Spike 1)_
- Freesound OAuth2 redirect: loopback URL accepted? _TBD (Spike 1 / Phase 4)_
