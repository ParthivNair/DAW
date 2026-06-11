# Research Request: Stack Verification & Build Plan for a Personal DAW

## Context

I'm a solo developer building a DAW for personal use, developed primarily with Claude Code. The concept: a simple, modern DAW in the spirit of Cakewalk Next — clean single-window UI, low friction — built on **Tracktion Engine + JUCE (C++)**.

The differentiating feature is **integrated online sound discovery**: searching online sound databases (starting with Freesound.org) from inside the DAW using natural-language/semantic search ("dark ambient pad, slow attack"), previewing results, and dragging them onto the timeline with license metadata preserved. Standard effects (EQ, filter, delay, reverb, compression) are required. Deep MIDI/scoring features are not a v1 priority.

Constraints and preferences:

- Commercial release is not planned; **GPLv3-compatible licensing is acceptable**, so prefer the best free/open tooling.
- [Primary OS: **Windows 11** — edit this line if different.] Cross-platform is nice-to-have, not a v1 requirement.
- Language/build: C++20, CMake. Solo developer + Claude Code.
- Simplicity over feature completeness — Cakewalk Next vibe, not Cubase.

Your job: verify and finalize the stack, audit licensing, research the sound-search feature in depth, and produce a phased build plan optimized for AI-assisted development. Today's date matters: prioritize information current as of **mid-2026**, prefer primary sources (official docs, repos, maintainer forums), include version numbers and dates, and clearly separate verified facts from inference. If research reveals that Tracktion Engine is a poor foundation today (abandoned, broken with current JUCE, etc.), say so directly and propose the best alternative.

## 1. Foundation: Tracktion Engine + JUCE (verify, don't assume)

- Current state of Tracktion Engine: latest release/branch, JUCE 8 compatibility, repo activity and maintenance health, documentation quality, and which official demo apps exist (playback, recording, plugin hosting, etc.).
- Exactly what the engine provides vs. what I must build: confirm it supplies the playback graph, edit/track/clip model, automation, recording, and plugin hosting — but **no UI** (no timeline view, piano roll, mixer, or waveform rendering). List everything UI-side I'm on the hook for.
- JUCE 8: current version and licensing tiers as of mid-2026 (GPL option, free-tier revenue thresholds, any recent license changes), plus known issues relevant to DAW-scale apps.
- Time-stretching: which algorithms ship with Tracktion by default (SoundTouch?), whether Élastique is commercial-only, and how Rubber Band (GPL) or signalsmith-stretch (MIT) would integrate. Recommend one for my licensing situation.
- Best learning resources: official docs, key forum threads, The Audio Programmer content, and open-source apps built on Tracktion Engine worth reading.

## 2. Licensing audit (deliverable: a table)

For each item: license type, cost for a personal/GPL project, obligations (attribution, source disclosure), and gotchas.

JUCE 8 · Tracktion Engine · Steinberg VST3 SDK (for **hosting**) · CLAP · Steinberg ASIO SDK (Windows low-latency) · Rubber Band · signalsmith-stretch · audio codec path (libsndfile, FFmpeg, or alternatives) · the chosen inference runtime (e.g., ONNX Runtime) · the chosen audio-embedding model weights (e.g., LAION-CLAP) · Freesound API terms of service.

Flag anything incompatible with a GPLv3 codebase, and anything restricting storage/redistribution of downloaded sounds.

## 3. The differentiator: online sound search (research this deepest)

- **Freesound API, in depth**: auth flows (token vs. OAuth2 for full-quality downloads), text search and filter syntax, whether content-based / similar-sound / AI search is already built into the API, preview URLs and formats, rate limits, attribution and ToS obligations for a client app, and license metadata fields (CC0 / CC-BY / CC-BY-NC) with how to filter on them.
- Other sound libraries that are **legally accessible programmatically** as of 2026 (e.g., Jamendo, BBC Sound Effects archive, Internet Archive audio, others you find) — and which explicitly forbid automated access (most commercial libraries like Splice). I only want API-sanctioned sources.
- Semantic search state of the art: the best current open audio-text embedding models (LAION-CLAP and any successors), model sizes, CPU inference cost, and realistic retrieval quality for short samples and loops.
- Practical architecture: query-time search via Freesound's own engine vs. building a local embedding index of downloaded/previewed sounds vs. hybrid. **Verify what Freesound already offers before recommending I rebuild it.**
- Local index tooling for a personal-scale library (tens of thousands of samples): SQLite + sqlite-vec vs. FAISS vs. usearch, plus sensible tagging/dedup approaches.
- Inference integration for a C++ app: ONNX Runtime embedded vs. a localhost Python sidecar service vs. other current options. Recommend whichever a solo dev can maintain most easily — a sidecar is acceptable.

## 4. Plugin hosting & built-in effects

- VST3 hosting via Tracktion/JUCE: maturity, crash-protection/sandboxing options, and using pluginval to smoke-test hosting.
- **CLAP hosting** status in JUCE/Tracktion as of mid-2026. Note: clap-juce-extensions targets plugin *creation*; hosting is a separate problem — what's the current realistic path, if any?
- Built-in effect plan: what Tracktion ships with, plus recommended open-source DSP to wrap for a starter set (JUCE dsp module, chowdsp_utils, Airwindows ports): EQ, filter, compressor, delay, reverb, limiter.

## 5. Claude Code-optimized development setup

The project will be built primarily with Claude Code, so research the workflow itself:

- Current best practices (mid-2026) for Claude Code on large C++/CMake codebases: CLAUDE.md conventions, repo structure for AI navigability (small files, clear module boundaries), useful MCP servers or skills (e.g., docs lookup), and effective use of subagents if relevant.
- Fast iteration: CMake + Ninja + ccache presets for JUCE/Tracktion projects, targeting sub-30-second incremental builds; how existing projects structure this.
- **Automated verification Claude can run without human listening — this is critical**: offline render-to-WAV tests, golden-file comparison strategies (sample-exact vs. tolerance vs. spectral), null tests for DSP correctness, RMS/THD checks, plugin-hosting smoke tests, and whether UI screenshot testing is feasible for JUCE.
- Real-time-safety tooling: RealtimeSanitizer (RTSan) in recent Clang, applicability of TSan, recommended lock-free queue libraries (farbot, moodycamel, etc.), and conventions for enforcing "no allocations/locks/IO on the audio thread."
- CI: a GitHub Actions setup for Windows JUCE builds (optional cross-platform matrix), with caching.

## 6. Landscape check (keep this brief)

- Current status of Cakewalk Next and Sonar as of mid-2026 — my UI reference point.
- Waveform Free — Tracktion's own DAW — as proof of what the engine can do and a UX reference.
- Open-source DAWs worth mining for patterns (Ardour, Zrythm, Stargate, LMMS, OpenDAW): note which have readable code for waveform rendering, undo systems, and editor interactions.
- Splice / Loopcloud purely for search-UX inspiration (not API use).

## 7. Deliverables

1. **Final recommended stack** with specific versions and one-line justifications; list alternatives where the call is close.
2. **Licensing table** from Section 2.
3. **Risk register**: the top 8–10 technical risks for this stack with mitigations (e.g., plugin GUI embedding quirks on Windows, plugin delay compensation behavior, Tracktion documentation gaps, model quality for semantic search).
4. **Phased roadmap** where every phase ends in something usable, each with explicit *automated* acceptance checks Claude Code can run. Suggested shape — adjust if research says otherwise:
   - Phase 0 — project skeleton builds; sine wave plays through the audio device.
   - Phase 1 — import audio, arrange clips on a timeline, play, render to WAV.
   - Phase 2 — mixer, built-in effects, VST3 hosting.
   - Phase 3 — Freesound text search + preview + drag-to-timeline + license tracking.
   - Phase 4 — semantic search + local sample library index.
   - Phase 5 — recording, automation UI, polish.
5. **Curated resource list** (docs, example repos, forum threads, videos) formatted for dropping into project knowledge for Claude Code.

Throughout: cite sources with dates, call out anything that changed in the last 12 months, and explicitly flag any claim you could not verify.
