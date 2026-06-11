# 07 — Deliverables: Stack, Risks, Roadmap, Resources

Synthesis of research files 01–06. Date: **2026-06-11**. See the individual files for sources and verification status; this file repeats only the conclusions.

---

## 1. Final recommended stack

| Layer | Choice | Version (June 2026) | One-line justification |
|---|---|---|---|
| Engine | **Tracktion Engine** | v3.2.0 release / `develop` pinned at a known-good SHA (last commit Feb 2026) | Only maintained open-source engine providing the full playback graph, edit model, automation, recording, plugin hosting, and rendering; powers Waveform 13.5 |
| Framework | **JUCE 8** (engine's pinned submodule) | 8.0.13 (May 2026); use ≥ 8.0.11 | Required by Tracktion; ≥ 8.0.11 bundles the MIT VST3 SDK 3.8 |
| Language/build | C++20, CMake + **Ninja + ccache**, CMakePresets | — | Sub-30 s incremental loops are achievable and critical for AI-assisted iteration (see 05) |
| Plugin formats | **VST3 + AU for v1** (punt CLAP) | VST3 SDK 3.8 (MIT); AU via system framework | JUCE hosts both maturely; AU is the macOS-native format (many AU-only/AU-first vendors) and near-free to add; no realistic CLAP hosting path in JUCE/Tracktion as of mid-2026 (see 04) |
| Audio I/O (macOS) | **CoreAudio** | macOS system framework (no SDK) | Native low-latency on macOS, built into JUCE `juce_audio_devices`; a system framework with no SDK or licensing question |
| Time-stretch | **Rubber Band 4.x** (GPLv2+); signalsmith-stretch (MIT) as hedge | — | Official engine integration hooks (`TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND`); SoundTouch bundled fallback; Élastique is commercial-only |
| Built-in FX | Tracktion internal plugins (4-band EQ, compressor, delay, reverb, chorus, phaser…) + **Airwindows via `TRACKTION_AIR_WINDOWS=1`** (MIT, 200+ FX); chowdsp_utils / sst-effects as upgrade tier | — | Day-one usable effects with zero DSP work; quality upgrades later without licensing pain |
| Codecs | libsndfile (LGPL-2.1) + JUCE built-ins (WAV/AIFF/FLAC/Ogg) + macOS Core Audio (`CoreAudioFormat`) for MP3/AAC decode | — | Covers everything needed; skip FFmpeg unless a concrete need appears (compliance surface) |
| Sound search | **Freesound API v2** (`/apiv2/search/`, post-Nov-2025 endpoint) with `similar_to` + `similarity_space=laion_clap` | — | Freesound already runs LAION-CLAP (`630k-audioset-fusion-best`) server-side — don't rebuild their index |
| Embedding model | **LAION-CLAP `laion/clap-htsat-fused`** — text tower in-process via ONNX Runtime; audio tower via Python sidecar (later phase) | ONNX Runtime 1.26.0 (MIT) | Must match Freesound's checkpoint exactly; text tower exports cleanly, audio tower's mel front-end doesn't |
| Local vector index | **SQLite + sqlite-vec** (v0.1.9) | — | Brute-force KNN is ~ms at personal scale; metadata DB is SQLite anyway; usearch is the escape hatch |
| Dedup | Freesound `md5` field + local md5 + Chromaprint (loops) + embedding cosine (one-shots) | — | Layered exact/near-dupe detection (see 03 §5) |
| RT-safety | RealtimeSanitizer (Clang/LLVM ≥ 20) **natively on macOS** (Homebrew LLVM clang) + moodycamel ReaderWriterQueue behind an `src/rt/` facade | — | RTSan supports macOS, so it runs in your normal build/test lane (see 05) |
| Plugin validation | **pluginval** (GPLv3) at strictness 10 in CI | — | Tracktion's own validator; exit-code based, CI-ready |
| Project license | **GPL-3.0-or-later**, with `THIRD_PARTY_LICENSES` noting JUCE is AGPLv3 | — | JUCE 8's open-source option is AGPLv3, not GPLv3 — compatible, but say so correctly |

Close calls: signalsmith-stretch instead of Rubber Band if avoiding GPL-only deps matters more than the official integration; usearch instead of sqlite-vec if the library outgrows brute force; MS-CLAP (MIT) instead of LAION-CLAP only if abandoning Freesound's server-side similarity space.

## 2. Licensing summary

Full table in [02-licensing-audit.md](02-licensing-audit.md). Headlines:

- **No component blocks a GPLv3 release.** The audio path is CoreAudio (a system framework — no SDK, no licensing question) and VST3 is now MIT (bundled in JUCE ≥ 8.0.11).
- **JUCE 8's open-source tier is AGPLv3** — one-way compatible with GPLv3; distributed builds carry AGPL terms (practically inert for a desktop app).
- **Freesound is the main obligations surface:** API free for non-commercial use only; OAuth2 required for original-quality downloads; attribution to Freesound + uploaders mandatory; no replicating/mirroring the catalog; CC-BY-NC sounds taint commercial use of user projects — default search filters to CC0 + CC-BY.
- Open-source app cannot embed a secret API key — each user registers their own (established practice).

## 3. Risk register

| # | Risk | Likelihood / impact | Mitigation |
|---|---|---|---|
| 1 | **UI scope explosion** — engine provides *no* UI; timeline, waveforms, piano roll, mixer, automation editing, DnD, undo wiring are all hand-built; this is the documented solo-DAW killer (Meadowlark) | High / High | Cakewalk-Next-level minimalism; phase gates; mine Ardour (waveform peak cache, MementoCommand undo) and Qtractor (command-pattern undo) for patterns — patterns only, AGPL/GPL code not copied |
| 2 | **Tracktion Engine maintenance model** — 3-person team, no third-party PRs accepted, ~4-month commit gap as of June 2026 | Medium / High | Pin a known-good SHA; vendor everything; keep local patches; forum is the support channel; engine is GPL so a fork is the last resort |
| 3 | **Text-vector `similar_to` trick is undocumented** — semantic search hinges on Freesound accepting locally computed CLAP *text* embeddings as `similar_to` vectors | Medium / High | 1-day Python spike before any C++ work (03 §7.1); fallbacks: documented `similar_to=<id>` "more like this" + lexical search + descriptor filters still make a strong feature |
| 4 | **CLAP weak on one-shots and blind to BPM/key** | High / Medium | Hybrid query parser: route bpm/key/loop/one-shot terms to Freesound's symbolic filters (`bpm`, `tonality`, `loopable`, `single_event`), CLAP handles only the timbral residue |
| 5 | **Plugin editor embedding / Retina on macOS** — `NSView` parenting/clipping quirks, mixed Retina/non-Retina scaling, plugin-initiated resizes | Medium / Medium | Give each editor its own top-level window; test marquee plugins (incl. native AU) early across a 2× + 1× display setup; keep JUCE renderer toggle (CoreGraphics / software / optional Metal layer) |
| 6 | **Plugin scan crashes** | High / Low | Enable Tracktion's out-of-process scanning (`setUsesSeparateProcessForScanning`) from day one; defer runtime sandboxing (Waveform's is proprietary) |
| 7 | **ONNX export of CLAP audio tower** — mel front-end not exportable; bit-exactness required | High / Medium (deferred phase) | Python sidecar runs reference code; convmelspec or pre-exported DCLAP ONNX as native escape; text tower (the critical path) exports cleanly |
| 8 | **Real-time bugs invisible to tests** (allocations/locks on the audio thread) | Medium / High | RTSan runs natively on macOS (Homebrew LLVM clang ≥ 20) + `[[clang::nonblocking]]`; grep tripwires + AI-review checklist on every diff; all RT communication behind one `src/rt/` facade |
| 9 | **Preview-quality audio contaminating projects** — lossy previews on the timeline, ms-level offset shifts after original replacement | Medium / Medium | Explicit placeholder state per clip; OAuth2 "upgrade to original" flow with cross-correlation re-alignment; warn at export if placeholders remain |
| 10 | **License contamination of user projects** (CC-BY-NC, Sampling+ stragglers) | Medium / Medium | Default filter `license:("Attribution" OR "Creative Commons 0")`; license badge in results; attribution stored in project file; export-time warning + auto-generated credits file |

## 4. Phased roadmap (every phase ends usable, with checks Claude Code can run)

**Phase 0 — Skeleton.** CMake+Ninja+ccache presets, JUCE+Tracktion as pinned submodules, `daw_core` lib + console test target, CI (macOS primary lane + native RTSan/sanitizer jobs, optional Linux cross-check). Sine wave through the CoreAudio device.
*Checks:* clean configure+build in CI; unit-test binary runs; offline render of a 440 Hz sine via `Renderer::renderToFile` → assert RMS/frequency within tolerance.

**Phase 1 — Timeline.** Import WAV/FLAC/MP3, arrange/trim/move clips, transport, render to WAV. Waveform display via `SmartThumbnail` data. Undo from the start (engine state-based undo + UI gesture wiring).
*Checks:* golden-file render tests (tolerance-based) for clip placement/trim/gain; null test re-render after undo/redo round-trip; `createComponentSnapshot` PNG regression for the timeline.

**Phase 2 — Mixer + FX + VST3/AU.** Mixer strip UI, Tracktion built-in FX + Airwindows set, VST3 + AU hosting with out-of-process scanning, PDC verified.
*Checks:* render-with-EQ golden tests; PDC null test (latency-inducing plugin vs dry, delay-compensated diff ≈ silence); host smoke test loading pluginval-validated free plugins headlessly; pluginval strictness 10 over the built-in set.

**Phase 3 — Freesound (token auth).** Search box → `/apiv2/search/` with filters, license filter defaulting away from NC, HQ-ogg preview streaming, drag-to-timeline with author/license/URL metadata persisted, credits export. Fork `MTG/freesound-juce` as a starting point.
*Checks:* API client tests against recorded fixtures (no live API in CI); project-file round-trip preserves license metadata; export warning triggers on NC asset (unit-testable).

**Phase 4 — Semantic + local library.** ONNX text encoder → `similar_to` vector search; "more like this" per result; OAuth2 original-quality upgrade flow; Python sidecar embeds user library into sqlite-vec; unified local+online results; md5/chromaprint dedup.
*Checks:* text-encoder output matches Python reference within 1e-4; KNN retrieval test over a fixture library (known query → known top-k); dedup unit tests.

**Phase 5 — Recording, automation UI, polish.** Audio recording, automation lanes (engine supports read/write/touch/latch since v3.2), keyboard-driven workflow polish.
*Checks:* record-from-virtual-device render test; automation render golden test (ramp → expected gain curve in output, sample-tolerance).

## 5. Curated resource list

**Foundation (see 01 for the full annotated list)**
- Tracktion Engine repo + DemoRunner (15 demos — the de-facto docs): https://github.com/Tracktion/tracktion_engine
- Dave Rowland, ADC23 "Why You Shouldn't Write a DAW"; ADC20 Tracktion Graph talk
- JUCE forum, Tracktion Engine category: https://forum.juce.com/c/tracktion-engine
- fluid-music/cybr — best third-party Tracktion Engine codebase to read
- The Audio Programmer Tutorial 43 + Discord

**Sound search (see 03)**
- Freesound API docs: https://freesound.org/docs/api/ (source of truth: `MTG/freesound` `_docs/api/source/*.rst`)
- MTG/freesound-juce (official JUCE client): https://github.com/MTG/freesound-juce
- LAION-CLAP: https://github.com/LAION-AI/CLAP · HF `laion/clap-htsat-fused`
- sqlite-vec: https://github.com/asg017/sqlite-vec · convmelspec: https://github.com/adobe-research/convmelspec
- SLAP (Jan 2026, watch for checkpoints): https://arxiv.org/abs/2601.12594 · MAEB benchmark: https://arxiv.org/abs/2602.16008

**Hosting & FX (see 04)**
- pluginval: https://github.com/Tracktion/pluginval
- Airwindows consolidated ports; chowdsp_utils; sst-effects
- JUCE AudioPluginHost source (ChildProcessCoordinator scanning pattern)

**Workflow & testing (see 05)**
- Claude Code docs (CLAUDE.md, hooks, subagents): https://code.claude.com/docs
- pamplejuce (JUCE+CMake+CI template) and sudara's blog (JUCE CMake, testing audio code)
- RealtimeSanitizer (LLVM 20+ docs); moodycamel ReaderWriterQueue; farbot (patterns)
- Tracktion `Renderer` API for offline render tests

**Landscape (see 06)**
- Waveform Free 13.5 (engine ceiling proof); Cakewalk Next (UI reference)
- Ardour 9.7 source (waveform peak cache, MementoCommand); Qtractor 1.6 (compact undo); openDAW (canvas waveform rendering)
- Splice/Loopcloud search UX patterns (key/BPM-synced preview, similar-sound, drag-conformed audio)
