# Foundation Layer Research: Tracktion Engine + JUCE 8 (as of June 2026)

Research date: 2026-06-11. Sources are primary (GitHub repos, official sites, JUCE forum) unless noted. Items I could not directly verify are flagged inline with **[unverified]** or **[inference]**.

---

## 1. Current State of Tracktion Engine

Repo: https://github.com/Tracktion/tracktion_engine — ~1.4k stars, ~35 open issues, dual GPL/commercial license, **default branch is `develop`** (fetched 2026-06-11).

### Releases / tags
| Tag | Date |
|---|---|
| v3.2.0 | May 15, 2025 (latest tagged release; adds automation read/write/touch/latch modes, AutomationCurve bypass) |
| v3.1.0 | Feb 5, 2025 |
| v3.0 | Jul 16, 2024 |
| v2.1.0 | Jan 10, 2023 |
| v1.2.0 | Jan 14, 2022 |
| 1.0.0 | Dec 8, 2020 |

Source: https://github.com/Tracktion/tracktion_engine/tags (fetched 2026-06-11). Note: one GitHub page render reported v3.2.0 as "May 15, 2024"; the tags listing says **May 15, 2025**, which is consistent with v3.1.0 (Feb 2025). Treat the 2025 date as correct, 2024 as a fetch artifact.

### Maintenance health
From the develop commit feed (https://github.com/Tracktion/tracktion_engine/commits/develop.atom, fetched 2026-06-11):

- **Most recent commit: Feb 18, 2026** ("Avoided a crash on shutdown"). That is a **~4-month gap as of June 2026** — the longest quiet period in the repo's recent history. Activity through Jan–Feb 2026 was healthy and substantive:
  - **ARA2 support merged** (Feature/ara2 #273, Feb 9, 2026)
  - **DAWproject import support** ("phase one", MIDI controller support — Feb 4, 2026)
  - "Updated juce" / "Updated for latest juce" (Feb 13, 2026)
  - CMake option to include targets in a parent project (Feb 10, 2026)
  - Fix for a rare use-after-free race in audio recording (Feb 7, 2026)
- Committers are essentially three people: **drowaudio** (Dave Rowland, lead), **julianstorer** (Jules Storer, Tracktion/JUCE founder), **FigBug** (Roland Rabien). This is a small-team project sustained by a commercial product (Waveform), not a broad community project.
- **Third-party PRs are not accepted** ("due to copyright restrictions" — README). You cannot fix engine bugs upstream yourself; you report on the JUCE forum and wait, or carry a fork/patch.
- CI: Build, codecov, and a "JUCE compatibility" workflow all passing on develop.

### JUCE 8 compatibility
- The engine vendors JUCE as a **git submodule** at `modules/juce` (commit `7c89e11f...` on develop as of June 2026). The "Updated juce" commits (Feb 2026) show it tracks recent JUCE 8. I could not pin the exact JUCE tag the submodule points to **[unverified — check `git submodule status` after cloning]**.
- Practical rule: **build against the engine's pinned JUCE submodule**, not whatever JUCE you have lying around. The engine reaches into JUCE internals and the team only tests against the pinned version; mixing versions is a classic source of breakage reported on the forum.
- Requires **C++20** (README) — matches this project's toolchain.

### Documentation quality
- **API reference (Doxygen):** https://tracktion.github.io/tracktion_engine/ — exists and is generated from headers, but it is mostly *signatures with one-line comments*. Many classes have no usage guidance.
- **Tutorials:** only **four** markdown tutorials in `/tutorials` (fetched 2026-06-11): `01 - PlaybackDemo`, `02 - PitchAndTimeDemo`, `03 - StepSequencerDemo`, `04 - DistortionEffectDemo`. They date from the early days of the project and do not cover recording, automation, clip launching, ARA, or DAWproject.
- **The real documentation is the demo source code and the Waveform behavior.** Expect to read engine source frequently. This is the most commonly cited pain point on the forum.

### Demo / example apps (develop branch, June 2026)
The old standalone demos were consolidated into **DemoRunner** (`examples/DemoRunner/demos/`), which contains 15 single-header demos:

`PlaybackDemo`, `RecordingDemo`, `MidiRecordingDemo`, `MidiPlaybackDemo`, `PluginDemo`, `SimplePluginDemo`, `PitchAndTimeDemo`, `StepSequencerDemo`, `PatternGeneratorDemo`, `DistortionEffectDemo`, `IRPluginDemo`, `ClipLauncherDemo`, `ContainerClipDemo`, `GlobalQuantiseDemo`, `AbletonLinkDemo`

Plus, at `examples/` top level: **EngineInPluginDemo** (engine hosted inside a plugin, syncing to host timeline), **Benchmarks**, **TestRunner**, and `common/` (notably `Utilities.h` / `Components.h`, which contain the closest thing to reference UI code: thumbnail drawing, simple track/clip components).

---

## 2. What the Engine Provides vs. What You Must Build

### Provided (verified from FEATURES.md, fetched 2026-06-11)
- **Playback graph**: `tracktion_graph` module — multi-CPU lock-free processing, automatic plugin delay compensation (see Dave Rowland's ADC20 talk "Introducing Tracktion Graph").
- **Edit/Track/Clip data model**: `Edit` (ValueTree-backed, serializes to `.tracktionedit` XML), audio/MIDI/step/marker/container clips, folder & submix tracks, tempo/time-sig curves, markers, time warping.
- **Automation**: bezier-curve automation, read/write/touch/latch modes (v3.2.0), modifiers (LFO, envelope follower, breakpoint, MIDI tracker), macro parameters.
- **Recording**: audio (with punch, formats) and MIDI recording, including retrospective record; the Feb 2026 commits show active hardening here.
- **Plugin hosting**: built-in plugins (volume/pan, EQ, delay, reverb, sampler, 4OSC synth, LevelMeasurer, etc.), external VST3/AU/(LV2) hosting via JUCE, plugin racks, sidechaining; **ARA2 as of Feb 2026 (develop only, post-v3.2.0)**.
- **Clip launcher**: scene-based non-linear launching with follow actions and global quantise (ClipLauncherDemo).
- **Rendering/export**: background-thread rendering, stem/clip rendering, MIDI export.
- **Other**: Ableton Link, MTC/MMC sync, control surface support (MCU etc.), DAWproject import (develop, Feb 2026), undo via `juce::UndoManager` integrated with the ValueTree model.
- **Non-visual UI helpers**: `tracktion::engine::SmartThumbnail` (wraps `juce::AudioThumbnail`, background-generates and caches waveform data per `AudioFile`, triggers component repaints as it loads — https://tracktion.github.io/tracktion_engine/classtracktion_1_1engine_1_1SmartThumbnail.html); `LevelMeasurer` for metering data; selection management classes (`SelectionManager`).

### NOT provided — FEATURES.md states it verbatim: *"it doesn't provide any kind of UI"*
Everything visual is on you. Concretely, for a usable DAW you must build:

1. **Timeline / arrangement view** — track lanes, time ruler, bars/beats vs. seconds display, zoom/scroll (incl. pixel↔`TimePosition` mapping, which gets fiddly with tempo curves), playhead drawing and auto-follow, loop/punch region UI.
2. **Waveform rendering** — `SmartThumbnail` gives you the data + repaint callbacks, but *you* write the `Graphics` drawing, channel layout, zoom-level handling, and caching strategy. Forum threads ("Draw waveform is too slow", forum.juce.com/t/draw-waveform-is-too-slow/56833; "Thumbnail Drawing", /t/thumbnail-drawing/49450) show naive implementations are slow at DAW scale — you'll need cached images / drawing only visible ranges.
3. **Clip components** — drawing, selection, drag-to-move (with snapping), trim/loop handles, fade handles, crossfades, copy-drag, right-click menus.
4. **Piano roll / MIDI editor** — entirely yours: note drawing, velocity lane, drag/resize/paint tools, quantise UI, CC lanes. This is one of the largest single UI builds in a DAW.
5. **Mixer view** — channel strips, faders/pans bound to engine `AutomatableParameter`s, meters fed from `LevelMeasurer`, plugin slot UI, routing UI.
6. **Automation editing UI** — curve drawing/editing on tracks (engine stores the curves; you render and edit them).
7. **Plugin windows** — opening/embedding external plugin editors, window management, multi-window focus handling (JUCE provides the editor component; the windowing/lifecycle policy is yours).
8. **Drag-and-drop** — files from Explorer onto tracks (JUCE `FileDragAndDropTarget`), internal DnD of clips/plugins.
9. **Undo/redo wiring** — engine uses `juce::UndoManager` per Edit; you must decide transaction boundaries (`beginNewTransaction`) around UI gestures, and keep UI in sync by listening to ValueTree changes rather than assuming your action was the only mutation.
10. **Transport bar, browser, settings UI, keyboard shortcut system (`ApplicationCommandManager`), project/file management UI, plugin scanning UX** (out-of-process scanning to survive crashing plugins — JUCE provides pieces; the orchestration is yours **[inference from Waveform behavior + JUCE API]**).

The `examples/common/Components.h` + `Utilities.h` files and the DemoRunner demos (especially RecordingDemo, MidiRecordingDemo) contain minimal versions of items 1–3 and are the canonical starting point.

---

## 3. JUCE 8 in Mid-2026

### Version
- Current release: **JUCE 8.0.13**, announced on the JUCE forum ~3 weeks before 2026-06-11, i.e. **late May 2026** (https://forum.juce.com/t/juce-8-0-13-released/68851; 240+ commits since 8.0.12, focus on OS/compiler/SDK compatibility — including **current Xcode / macOS SDK and Apple-silicon support**, relevant to your macOS setup). GitHub releases page render returned garbled dates for older 8.0.x patches **[dates of intermediate patches unverified]**.

### Licensing — important change vs JUCE 7
- **JUCE 8 switched its open-source license from GPLv3 to AGPLv3** (dual AGPLv3/commercial — https://github.com/juce-framework/JUCE/blob/master/LICENSE.md, fetched 2026-06-11). This was announced in 2024 and generated long forum threads (https://forum.juce.com/t/important-changes-to-the-juce-end-user-licence-agreement-for-juce-8/60947 and /t/amendments-to-the-juce-end-user-licence-agreement-for-juce-8/61265 — the EULA was amended after community pushback).
- **Impact on this project**: GPLv3 and AGPLv3 are one-way compatible — you may combine GPLv3 code (Tracktion Engine) with AGPLv3 code (JUCE 8); the combined work is effectively governed by AGPLv3's network-use clause for the JUCE parts. For a personal, locally-run DAW this is a non-issue; if you ever distribute, releasing the whole app under (A)GPLv3 terms satisfies both. Tracktion Engine itself remains **GPLv3 (or commercial)** and its README reminds you that JUCE needs its **own** license — two separate dual-license decisions.
- Commercial tiers (if you ever go closed-source): Starter — **free under US$50k annual revenue/funding**, Indie — under US$500k, ~US$40/mo, Pro — no limit, ~US$130/mo; JUCE 8 dropped the splash-screen requirement on the free tier. Figures from forum.juce.com/t/revenue-limits-for-juce-tiers/61058 and juce.com/get-juce **[juce.com blocked direct fetch (403); figures cross-checked against two forum sources but not against the live EULA — verify before any commercial decision]**. Note revenue counts *gross, all JUCE-derived income including donations*.

### Known JUCE 8 issues relevant to a DAW (macOS focus)
On macOS, JUCE renders through **CoreGraphics** by default (CPU-side drawing; since recent JUCE the paint path is asynchronous). The macOS renderer is mature and quiet, but there are still things to know for a DAW-scale UI:

- **CoreGraphics is software (CPU) rasterization.** It is robust and battle-tested, but very heavy partial-repaint workloads (hundreds of components, meters, scrolling waveforms at high refresh) are CPU-bound. Profile early; the usual JUCE wins apply — repaint only dirty regions, cache rendered waveform strips as images, avoid full-component `repaint()` on meter ticks.
- **Optional Metal layer renderer**: JUCE can render via `CoreGraphicsMetalLayerRenderer` (CoreGraphics rasterizes on the CPU, then uploads the result as a texture to a `CAMetalLayer` on the GPU), gated behind `JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS`. It can help or *hurt* depending on how much CoreGraphics is redrawing, and it has its own history of glitch reports on the forum — treat it as an opt-in experiment, not a default. (forum.juce.com/t/more-issues-with-juce-coregraphics-render-with-multiple-paint-calls-and-metal/52503) **[behavior is version-sensitive — benchmark on your hardware]**.
- **Retina / HiDPI** is handled cleanly by JUCE on macOS (2× backing-scale `NSView`s); you mostly get this for free. Still test on a mixed Retina + non-Retina external-display setup.
- **Large cached images** still have GPU/CoreGraphics size ceilings — relevant to long cached waveform strips; tile your waveform caches rather than allocating one enormous image.
- **Escape hatch (verified API)**: you can force the pure software renderer per-window via the peer's `setCurrentRenderingEngine()`, or globally — build your UI so you can A/B renderers (CoreGraphics vs. software, and the Metal layer if you enable it).
- Other JUCE 8 changes worth knowing: new font/text rendering (Harfbuzz-based — API breaking changes around `Font` construction vs JUCE 7; deprecations you'll hit in old sample code), new animation module, WebView-based UI support (don't use for a DAW timeline), and `juce::AudioThumbnail` unchanged and still the standard waveform-summary tool underneath Tracktion's `SmartThumbnail`.

---

## 4. Time-Stretching Options

Verified directly from `modules/tracktion_engine/timestretch/tracktion_TimeStretch.{h,cpp}` on develop (fetched 2026-06-11).

### What the engine supports (TimeStretcher::Mode enum)
- `soundtouchNormal` / `soundtouchBetter` — **SoundTouch** (LGPL-2.1) — **source is bundled** at `modules/tracktion_engine/3rd_party/soundtouch/`, enabled with `TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1`. Zero-setup option; quality is serviceable but dated (audible artifacts on polyphonic material, weak transient handling).
- `elastiquePro` / `elastiqueEfficient` / `elastiqueMobile` / `elastiqueMonophonic` + `elastiqueDirect*` — **zplane Élastique**: **commercial-only SDK**, not in the repo; the .cpp `#pragma comment(lib, "libelastiquePro.lib")` lines expect you to supply zplane's licensed libraries (`TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE`). This is what Waveform ships with. Not an option for a GPLv3 hobby project (zplane licensing starts in the thousands of EUR) — **confirmed commercial-only**.
- `rubberbandMelodic` / `rubberbandPercussive` — **Rubber Band Library**: integration hooks are **built in but the library is NOT bundled** (verified: `3rd_party/` contains only `airwindows` and `soundtouch`). Two paths:
  1. `TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND=1` + link a prebuilt librubberband (header expected at `3rd_party/rubberband/rubberband/RubberBandStretcher.h`), or
  2. additionally `TRACKTION_BUILD_RUBBERBAND=1` to compile it from source via RubberBand's single-file build (`3rd_party/rubberband/single/RubberBandSingle.cpp`) — i.e. **clone breakfastquay/rubberband into `modules/tracktion_engine/3rd_party/rubberband` and set two flags**. There's an explicit compile error "TRACKTION_BUILD_RUBBERBAND enabled but not found in the search path!" if you forget.
- `ara` — stretch via ARA plugins (e.g. Melodyne) — newly meaningful with the Feb 2026 ARA2 merge.

### Rubber Band status (mid-2026)
- https://github.com/breakfastquay/rubberband / https://breakfastquay.com/rubberband/ — current major version **4.x** (4.0 added the `RubberBandLiveShifter` API; the R3 "finer" engine introduced in 3.0 is high quality, broadly competitive with commercial stretchers). **Dual-licensed GPL / commercial** (https://breakfastquay.com/rubberband/license.html) — GPL is exactly compatible with this project. Note: RubberBandSingle.cpp builds with built-in FFT and no libsamplerate, avoiding extra GPL-dependency wrangling **[the no-libsamplerate detail is from RubberBand docs, not re-verified this session]**.

### signalsmith-stretch
- https://github.com/Signalsmith-Audio/signalsmith-stretch — header-only C++11 polyphonic pitch/time library, **MIT** licensed, actively maintained (mirror of signalsmith-audio.co.uk/code/stretch/). Excellent at pitch-shifting; time-stretch is best within ~0.75x–1.5x per its own README.
- **No integration exists in Tracktion Engine** — searched the timestretch sources and the web; no `signalsmith` references in the repo (verified 2026-06-11). You would have to implement your own `TimeStretcher::Stretcher` backend inside the engine module (the .cpp's pluggable structure makes this feasible but it's engine-source surgery you then maintain against upstream churn) **[feasibility = inference from code structure]**.

### Recommendation for this project
**Rubber Band (GPL) via the engine's built-in hooks**, with SoundTouch enabled as a zero-cost fallback/A-B reference. Rationale: license-compatible (GPLv3 project), best quality among the free options, officially supported by engine code paths (`rubberbandMelodic` is even the engine's default mode when the flag is set), and the from-source single-file build keeps linking trivial on macOS. Skip signalsmith-stretch unless you later want MIT-only licensing or its superior pitch-shifting for a specific feature.

---

## 5. Best Learning Resources

**Official**
- Tutorials (only 4): https://github.com/Tracktion/tracktion_engine/tree/develop/tutorials
- DemoRunner demo sources (the real tutorials): `examples/DemoRunner/demos/` — read `RecordingDemo.h`, `MidiRecordingDemo.h`, `PluginDemo.h`, `ClipLauncherDemo.h` first; `examples/common/Utilities.h` & `Components.h` for thumbnail/track/clip UI patterns.
- API docs: https://tracktion.github.io/tracktion_engine/
- FEATURES.md (capability map): https://github.com/Tracktion/tracktion_engine/blob/develop/FEATURES.md

**Forum** (the de facto support channel — engine bugs are reported here, not GitHub issues)
- Tracktion Engine category: https://forum.juce.com/c/tracktion-engine/27 — search before fighting any problem; e.g. "Real-time TimeStretching" (/t/real-time-timestretching/53635), "Draw waveform is too slow" (/t/draw-waveform-is-too-slow/56833), "Thumbnail Drawing" (/t/thumbnail-drawing/49450).

**Talks / videos (Dave Rowland is the canonical source)**
- *"Why You Shouldn't Write a DAW"* — David Rowland, ADC23 (YouTube) — literally the parent task's "hurdles" framing, from the engine's lead developer. Watch first.
- *"Introducing Tracktion Graph"* — ADC20: https://www.youtube.com/watch?v=Mkz908eP_4g — how the playback graph works.
- All of Dave's slide decks: https://github.com/drowaudio/presentations (ValueTrees, real-time tradeoffs, etc.)
- The Audio Programmer: podcast *"How DAWs Work w/ Dave Rowland"* (https://www.theaudioprogrammer.com/podcast/how-daws-work-w-dave-rowland-tracktion-ep-01) and *JUCE Tutorial 43 – Tracktion Engine Intro* (https://www.youtube.com/watch?v=Ko9RWpjUHPc — old API, concepts still valid); their Discord has a community of engine users.
- WolfTalk #023 podcast: *Building DAW Software with Dave Rowland* (2024): https://thewolfsound.com/talk023/

**Open-source code built on the engine**
- **fluid-music/cybr** (https://github.com/fluid-music/cybr) — headless Tracktion-Engine server; the best third-party example of driving Edits, plugins, and rendering programmatically. Pinned to an older engine version **[inference — project activity slowed; check before copying patterns]**.
- **debrisapron/tracktion-engine-hello-world** — minimal "make a sound" starter.
- Honestly, the ecosystem of open-source engine consumers is thin — the engine's own examples plus cybr are most of it. Waveform Pro (closed-source) is the reference for *what's possible*, not for code.

---

## Key Hurdles

1. **Documentation gap is the #1 tax.** 4 tutorials + thin Doxygen for a ~quarter-million-line engine. You will learn by reading engine source and forum archaeology. Budget for it; for AI-assisted development, keep the engine source checked out in-repo so Claude can read it (it's the documentation).
2. **`develop` vs `master` / API churn.** Releases are infrequent (v3.2.0 was May 2025) while develop moves (ARA2, DAWproject landed Feb 2026, post-release). Forum answers and old sample code often target older APIs (v2 introduced strongly-typed `TimePosition`/`BeatPosition`/`TimeDuration`, breaking nearly all v1 snippets; the namespace moved to `tracktion::engine`). Pick `develop` pinned to a known-good SHA, and update deliberately.
3. **Pinned JUCE submodule.** Use the engine's JUCE submodule, not standalone JUCE; mixing versions breaks. This also means you adopt JUCE upgrades on the engine team's schedule.
4. **No third-party PRs + 3-person team + 4-month commit silence (Feb→Jun 2026).** If an engine bug blocks you, your options are: forum report and wait, or local patch you maintain forever. The Feb 2026 gap is likely just a lull (history shows bursts), but it's the first sign worth monitoring **[interpretation, not fact]**.
5. **All UI is on you** — timeline, clips, piano roll, mixer, automation editing, plugin windows, DnD, undo gesture boundaries. The piano roll and a fast zoomable waveform timeline are the two biggest sub-projects; naive `AudioThumbnail` drawing won't survive DAW scale (cache rendered strips, draw visible ranges only, tile below CoreGraphics/GPU image-size limits).
6. **JUCE 8 CoreGraphics rendering on macOS** — mature, but CoreGraphics is CPU-side rasterization, so DAW-scale partial repaints (meters, scrolling waveforms) are CPU-bound. Keep a runtime renderer toggle (CoreGraphics / software / optional Metal layer) and profile plugin-window-heavy sessions early.
7. **Licensing bookkeeping** — JUCE 8 is AGPLv3 (changed from GPLv3 in JUCE 7), engine is GPLv3; fine for a personal GPL project, but it's *two* separate licenses, and any future commercial pivot means buying both JUCE and Tracktion Engine commercial licenses, plus zplane if you want Élastique.
8. **Time-stretch setup friction** — best free option (Rubber Band) isn't bundled; you must vendor the source and set `TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND` + `TRACKTION_BUILD_RUBBERBAND`. Easy, but undocumented outside the code itself.
9. **Threading/undo discipline** — the ValueTree-backed model must be mutated on the message thread; the engine handles audio-thread sync, but UI code that mutates state off-thread or forgets `UndoManager` transactions causes subtle corruption. (Standard engine-forum advice **[community consensus, not official doc]**.)

## Verdict

**Yes — Tracktion Engine remains the right foundation in mid-2026** for a solo, GPLv3-compatible personal DAW. Nothing else open-source provides a maintained, shipping-product-proven combination of playback graph + edit model + automation + recording + plugin hosting (now with ARA2 and DAWproject on develop); the alternatives are far lower-level (build a graph and model yourself on raw JUCE) or are full DAW codebases (Ardour, Zrythm, LMMS) that aren't embeddable engines. The honest costs: you're betting on a 3-person team that doesn't take PRs, documentation that is effectively "read the source," and 100% of the UI work — which is where most of a DAW's effort lives anyway. Mitigations: pin `develop` at a known-good SHA, vendor everything (engine, its JUCE submodule, RubberBand), keep a renderer toggle on macOS (CoreGraphics / software / optional Metal layer), and treat the DemoRunner demos + forum + Dave Rowland's talks as the missing manual. Monitor repo activity; if the post-Feb-2026 silence stretches past a year, re-evaluate — but the engine is GPL'd, so worst case you keep building on a frozen, working snapshot.
