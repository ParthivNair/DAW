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

## 2026-06-11 — Spike 1: Freesound semantic search is **GO**

Text-vector `similar_to` works against the live API. Locally embedded LAION-CLAP text
queries (checkpoint `630k-audioset-fusion-best`, via `laion_clap` `CLAP_Module(enable_fusion=True)`,
`load_ckpt(model_id=3)`) passed as `similar_to=[...512 floats]&similarity_space=laion_clap`
return genuine semantic nearest neighbors. Spike script: `tools/spike1_freesound_semantic.py`;
raw responses in `tools/spike1_out/` (git-ignored). Phase 4 builds the semantic path for real.

**Quality (11 queries, top-8 inspected, semantic vs lexical):**
- Clear semantic wins where lexical returns **zero** results: "rain falling on a tent",
  "birds singing in a forest at dawn", "dusty vinyl crackle loop 90 bpm" — all got 8/8
  on-target field recordings / textures.
- One-shots better than feared (Risk: CLAP short-sample weakness): "punchy kick drum one
  shot" and "deep 808 sub bass" returned 8/8 sub-second on-target one-shots; lexical found
  only 4 and 7 results total.
- Parity on tag-rich queries ("glass breaking", "cinematic riser whoosh"); semantic *worse*
  on "dark ambient drone" (engine/birds/fan noise mixed in) and "footsteps on gravel"
  (acoustic confusion: plastic-bag flapping #1). Lexical stays as a complementary path.
- "90 bpm" in the query is ignored by CLAP exactly as predicted → the Phase-4 hybrid
  query parser (bpm/key/loop terms → symbolic filters) is confirmed necessary.

**API mechanics discovered (binding for the C++ client):**
- **GET only** — POST to `/apiv2/search/` returns 405. Request line is capped at **4094
  chars** (gunicorn): a 5-decimal vector 400s. Fix: 3-decimal floats, trailing zeros
  stripped, commas/brackets NOT percent-encoded → ~3.4 KB URL. Rounding cost:
  cosine(full, 3dp) = 0.99998. Server L2-normalizes input (verified in live docs).
- `query` is **silently ignored** when `similar_to` is present (identical results/counts
  with and without it) → lexical and semantic cannot combine in one request; hybrid
  constraints must go in `filter`, which **does** compose (license + duration verified,
  0 violations).
- Semantic results are a capped candidate set (counts ~460–3600, not the whole catalog);
  filters apply within it. Result `score` = (1+cosine)/2 — usable as a relevance cutoff
  (verified numerically against our own vector math).
- `GET /sounds/<id>/analysis/` returns the stored 512-d vector under key `laion_clap`,
  alongside bpm/note/loopable/etc. descriptors.
- **`license` is returned as a deed URL** (e.g. `https://creativecommons.org/licenses/by/4.0/`),
  not the display name ("Attribution") used in *filter* values. Phase-3 badge mapping must
  translate. Legacy Sampling+ returns `http://creativecommons.org/licenses/sampling+/1.0/`
  (~11,414 sounds; `filter=license:"Sampling+"` works though undocumented) → default
  filter must be an allowlist (`"Attribution" OR "Creative Commons 0"`), unknown → hide/warn.
- Docs drift: none — live docs == repo master for the similarity feature (vector input,
  auto-L2-norm, `laion_clap` 512-d, `630k-audioset-fusion-best.pt` all documented live).
- Text embedding perf: ~25 ms/query on Apple-silicon CPU (PyTorch) — in-process ONNX
  text encoder (Phase 4) has generous headroom.

**Environment facts:** prototyping venv `~/venvs/daw` (Python 3.12); deps pinned in
`tools/requirements-spike1.txt` — `laion_clap` does not declare `torch`/`torchvision`/
`torchaudio` (install explicitly); `transformers` 5.x works; `torch>=2.6` needs a
`weights_only=False` shim for `load_ckpt`. Freesound credentials live in the vault-root
`.env` (`FS_CLIENT_ID`/`FS_CLIENT_SECRET`/`FS_REDIRECT_URL`; the client secret doubles as
the token-auth key). The registered OAuth2 redirect is Freesound's hosted
`permission_granted` page, i.e. the documented **paste-the-code fallback** — Phase 4
plans for that flow, not a loopback listener.

## Pending (fill in when decided)

- App name / bundle identifier: _TBD_
- Tracktion Engine pinned SHA + JUCE submodule SHA: _TBD (Spike 2)_
- ~~Semantic-search path GO/NO-GO~~: **GO** (see 2026-06-11 Spike 1 entry)
- ~~Freesound OAuth2 redirect~~: registered as the paste-the-code page (see Spike 1 entry)
