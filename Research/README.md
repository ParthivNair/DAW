# DAW Research — Hurdles & Challenges

Research findings for building a personal DAW on **Tracktion Engine + JUCE** (C++20, CMake, macOS, GPLv3-compatible), with integrated online sound discovery as the differentiating feature. Based on [`../daw-research-prompt.md`](../daw-research-prompt.md). Research current as of **June 2026**.

## Contents

| File | Category | Covers |
|------|----------|--------|
| [01-foundation-tracktion-juce.md](01-foundation-tracktion-juce.md) | Foundation | Tracktion Engine state, JUCE 8, what the engine provides vs. what must be built, time-stretching, learning resources |
| [02-licensing-audit.md](02-licensing-audit.md) | Licensing | License table for the full stack; GPLv3 compatibility red flags; sound storage/redistribution restrictions |
| [03-sound-search-and-freesound.md](03-sound-search-and-freesound.md) | Sound search (differentiator) | Freesound API deep dive, other API-sanctioned sources, CLAP-style audio-text embeddings, local index tooling, inference integration |
| [04-plugin-hosting-and-effects.md](04-plugin-hosting-and-effects.md) | Plugins & effects | VST3 hosting maturity, CLAP hosting reality, built-in effect plan, classic hosting hurdles |
| [05-claude-code-dev-workflow.md](05-claude-code-dev-workflow.md) | Dev workflow | Claude Code on large C++/CMake codebases, fast builds, automated audio verification without listening, real-time-safety tooling, CI |
| [06-landscape.md](06-landscape.md) | Landscape | Cakewalk Next/Sonar status, Waveform Free, open-source DAWs worth mining, search-UX inspiration |
| [07-deliverables.md](07-deliverables.md) | Synthesis | Final recommended stack, licensing table summary, risk register, phased roadmap with automated acceptance checks, curated resource list |
