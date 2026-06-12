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

## 2026-06-11 — Spike 2: engine builds & runs on this machine — **GO**

Tracktion Engine `develop` builds with CMake + Ninja + ccache on this Mac (M1 Pro 10-core,
16 GB, macOS 27 / Darwin 27, Xcode 26.4, Apple clang 21). DemoRunner launches and idles
cleanly; the engine's own headless TestRunner passes 52/53 doctest cases (1024 assertions,
1 known failure — see below). Spike clone: `~/src/tracktion_engine` (shallow, kept for
reference — the demos are the documentation).

**Pinned SHAs (use these for the Phase 0 submodule):**
- Engine: `2877b621f2fbee564d0696a616b86bf8ba8c8ab0` — develop HEAD, 2026-02-18,
  "Avoided a crash on shutdown", VERSION.md 3.2.0
- JUCE submodule (`modules/juce`): `7c89e11f6b7316c369f3d3f22227c60e816e738b` — JUCE 8.0.12

**Build times (Release, `-j8`, Apple clang, ccache via `CMAKE_<LANG>_COMPILER_LAUNCHER`):**
- DemoRunner cold (empty ccache): 10 s configure (incl. juceaide) + **41 s** compile+link
  (48 ninja edges — the umbrella-TU layout keeps the TU count tiny). Warm (deleted build
  dir, hot ccache): 5 s + **2 s** (96 % hit rate). The <30 s incremental budget for Phase 0
  looks very comfortable.

**Gotchas that bind Phase 0 (no engine-source patches needed):**
1. **JUCE submodule URL is SSH** (`git@github.com:juce-framework/JUCE.git` in `.gitmodules`)
   → recursive clone fails without GitHub SSH keys. A `url.…insteadOf` rewrite did NOT take
   effect for the already-initialized submodule; what worked:
   `git config submodule.modules/juce.url https://github.com/juce-framework/JUCE.git`
   then `git submodule update --init --recursive`. CI must do the same (or use SSH keys).
2. **SDK skew**: with both Xcode and CommandLineTools installed, default sysroot resolution
   picks the CLT SDK, which here is macOS **27** — newer than Xcode 26.4's clang → hard
   errors in SDK headers (`unknown attribute 'stack_protector_ignore'` from `os/signpost.h`).
   (`xcrun --show-sdk-version` itself errors on this machine.) Fix, mandatory in our presets:
   `-DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk`.
   With the matching SDK the unknown-attribute class disappears entirely.
3. **`-Werror` in the engine's example CMakeLists** (Release only) fails with Apple clang 21
   on three pre-existing warning classes in engine/JUCE code:
   `-Wimplicit-int-float-conversion` (~300 sites), `-Wtautological-overlap-compare` (1 site,
   a real bug, below), `-Wdeprecated-declarations` (`std::wstring_convert` in JUCE's bundled
   VST3 SDK). Workaround: `-Wno-error=<class>` downgrades (specific beats blanket `-Werror`
   in clang regardless of flag order), or strip `-Werror`. Our own targets set their own
   flags, so this only affects building the engine's examples/tests.
4. **Upstream bug found** (would be caught by their CI if it used clang 21):
   `modules/tracktion_engine/audio_files/tracktion_LoopInfo.cpp:546` —
   `if (len <= 1.0 && len > 60.0)` is always false; clearly meant `||`. Report on the
   Tracktion forum when convenient.

**TestRunner result (engine unit tests, headless, Release):** 53 doctest cases /
1024 assertions, **52 pass, 1 fail** (deterministic across two runs): the *EditClip* null test
(`tracktion_EditClip.test.cpp:103`) — a nested-edit render doesn't phase-cancel against its
source (RMS diff 0.692 where ≈0 expected); both absolute-level checks pass. The clip uses
auto-tempo (time-stretch path) and we built **without Rubber Band** (SoundTouch only;
engine CI vendors Rubber Band into `modules/3rd_party/rubberband`, which the TestRunner
CMakeLists probes for). Working hypothesis: SoundTouch's processing offset breaks
sample-aligned cancellation. **Re-check once Phase 0 wires Rubber Band**; if it still fails
then, investigate before trusting nested-edit renders.

**Outstanding [You] item:** DemoRunner is left running — open it, pick *PlaybackDemo*, and
confirm audio comes out of the speakers (no programmatic audio/screen capture was available
in this session).

## Pending (fill in when decided)

- App name / bundle identifier: _TBD_
- ~~Tracktion Engine pinned SHA + JUCE submodule SHA~~: pinned (see 2026-06-11 Spike 2 entry)
- ~~Semantic-search path GO/NO-GO~~: **GO** (see 2026-06-11 Spike 1 entry)
- ~~Freesound OAuth2 redirect~~: registered as the paste-the-code page (see Spike 1 entry)
