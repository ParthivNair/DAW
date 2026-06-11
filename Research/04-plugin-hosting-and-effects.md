# 04 — Plugin Hosting & Built-In Effects

**Research date:** 2026-06-11 · **Context:** Personal DAW on Tracktion Engine + JUCE 8, C++20, macOS primary, GPLv3 codebase.
**Legend:** ✅ verified against primary source · ⚠️ inference / community report · ❓ could not verify.

---

## 1. VST3 hosting via JUCE / Tracktion Engine

### 1.1 Maturity

✅ JUCE's `juce_audio_processors` VST3 hosting is mature and battle-tested — it powers Tracktion Waveform, pluginval, and the `AudioPluginHost` example. Tracktion Engine wraps it in `te::ExternalPlugin`, so as an engine user you mostly never touch `AudioPluginInstance` directly: scanning, instantiation, parameter exposure, automation, and state persistence (plugin state is serialized into the Edit's ValueTree) are handled by the engine (⚠️ state-in-Edit detail is from engine source/forum knowledge, not re-verified this session).

### 1.2 Known pain points (the classic hurdles)

**Editor view embedding on macOS.** Hosted VST3/AU editors are native **`NSView`s** parented into your window (AU editors come from the plugin's view-controller; VST3 editors are Cocoa `NSView`s via `IPlugView`). JUCE handles the parenting, but the forum history shows recurring issues that apply cross-platform: editors opening at the wrong size, glitching during resize, and crashes when closing hosted editors. See [VST3 plugin editor resizing glitch/issues](https://forum.juce.com/t/vst3-plugin-editor-resizing-glitch-issues/44170), [Crash closing hosted plugin editor](https://forum.juce.com/t/crash-closing-hosted-plugin-editor/18566), [Scale/size issue with plugin window from Plugin Host example](https://forum.juce.com/t/scale-size-issue-with-plugin-window-very-very-small-from-plugin-host-example/18240). Rule of thumb: give each plugin editor its own top-level `DocumentWindow` (what Waveform does) rather than embedding editors inside complex component hierarchies — far fewer z-order/focus/keyboard problems, and it sidesteps macOS `NSView` clipping/layer-backing quirks when a plugin view lives inside a deep hierarchy. (⚠️ recommendation is inference from forum consensus.)

**Retina / HiDPI.** Largely a solved problem on macOS: it uses integer backing-scale factors (1× or 2×), and JUCE's Cocoa peer handles the conversion, so plugin editors generally render at the correct size ([scaling a hosted plugin UI](https://forum.juce.com/t/scaling-ui-of-hosted-plugin/53043)). The edge cases worth testing: a Retina (2×) built-in display driving a non-Retina (1×) external monitor, dragging a plugin editor between the two, and AU/VST3 plugins that do their own internal scaling. Test against non-JUCE plugins (u-he, FabFilter, native AU instruments) on a mixed Retina + non-Retina setup early — bugs here are per-plugin. (⚠️ JUCE 8 specifics inferred; the issue class is ✅ well documented.)

**Plugin delay compensation (PDC).** ✅ Tracktion Engine handles PDC automatically. The engine's [FEATURES.md](https://github.com/Tracktion/tracktion_engine/blob/develop/FEATURES.md) lists "Perfect plugin delay compensation"; plugins report latency via `Plugin::getLatencySeconds()` and the tracktion_graph engine delays sibling nodes so summed paths align ([forum: How is internal plugin latency applied](https://forum.juce.com/t/how-is-internal-plugin-latency-applied-in-tracktion-engine/62110), [Custom te::Plugin and latency](https://forum.juce.com/t/custom-te-plugin-and-latency/53709)). History note: legacy-engine Waveform had real PDC bugs (bypassed plugins changing latency, aux/rack edge cases) fixed by the new tracktion_graph engine introduced around Waveform 11.1 with "PDC fixes and up to 30% performance improvement" ([KVR: Waveform 11.1.14 beta announcement](https://www.kvraudio.com/forum/viewtopic.php?t=551189), [KVR: Latency compensation in Waveform](https://www.kvraudio.com/forum/viewtopic.php?t=518334)). The open-source engine you'd use today is the new graph engine, so you inherit the fixed behavior. Caveat ⚠️: latency *changes* at runtime (e.g. plugin switches lookahead mode) require a graph rebuild; test that path.

**Plugin scanning crashes.** A bad plugin can crash or deadlock the scanner; in-process scanning therefore kills your DAW. Forum evidence: [Plugin scanning process always crashes the application](https://forum.juce.com/t/plugin-scanning-process-always-crashes-the-application/19006), [AudioPluginHost crashes scanning WaveShell (JUCE issue #997)](https://github.com/juce-framework/JUCE/issues/997), [VST3 plugin scanning crash protection](https://forum.juce.com/t/vst3-plugin-scanning-crash-protection/58485), [Coping with plug-in load/scan failures](https://forum.juce.com/t/coping-with-plug-in-load-scan-failures-e-g-exc-bad-access/49487).

### 1.3 What the engine gives you for scanning — big win

✅ **Tracktion Engine ships out-of-process, multithreaded scanning.** `te::PluginManager` (verified in [tracktion_PluginManager.h](https://github.com/Tracktion/tracktion_engine/blob/develop/modules/tracktion_engine/plugins/tracktion_PluginManager.h)) provides:

- `setUsesSeparateProcessForScanning()` / `usesSeparateProcessForScanning()`
- `startChildProcessPluginScan()` — "called by a child process in the app's start-up code, to invoke the actual scan" (you re-launch your own exe with a scan argument)
- `setNumberOfThreadsForScanning()` — parallel scanning
- `abortCurrentPluginScan` callback

✅ JUCE itself also demonstrates the pattern: `AudioPluginHost` contains a `Superprocess` class built on `ChildProcessCoordinator` implementing a `KnownPluginList::CustomScanner` that scans in a child process (verified in [MainHostWindow.cpp](https://github.com/juce-framework/JUCE/blob/master/extras/AudioPluginHost/Source/UI/MainHostWindow.cpp)). So: **use the engine's separate-process scanner from day one**; it converts the #1 DAW-crash source into a logged blacklist entry.

### 1.4 Crash protection / sandboxing at *runtime*

- ✅ JUCE does **not** offer out-of-process plugin *hosting* (only the scanning pattern above). Running plugins in a separate process with shared-memory audio IPC is a build-it-yourself project. ([Sandboxing an AudioProcessor?](https://forum.juce.com/t/sandboxing-an-audioprocessor/14178))
- ✅ **Tracktion Waveform 11.5+ ships per-plugin sandboxing**: selected plugins run in a separate process; if one crashes it's deactivated instead of killing the DAW ([Waveform Pro features](https://www.tracktion.com/products/waveform-pro-features), [KVR: Waveform 11.5 sandboxing](https://www.kvraudio.com/forum/viewtopic.php?t=591961), [video: Waveform 11.5 plugin sandboxing](https://www.youtube.com/watch?v=xOvgJW36VJc)). ⚠️ This sandbox layer is **proprietary Waveform code, not part of the open-source engine** — don't expect to inherit it. Sandboxing also has correctness costs ([JUCE forum: Waveform sandbox bug with non-automatable VST3 params](https://forum.juce.com/t/possible-tracktion-waveform-11-5-sandbox-bug-with-non-automatable-audio-parameters-and-vst3/44878)).
- ✅ What real DAWs do: Reaper (optional bridging), Bitwig (per-plugin/per-vendor process isolation), Logic (AUv3/M1 out-of-process), Cubase (none — frequent user requests: [Steinberg forum thread](https://forums.steinberg.net/t/solution-to-crashes-by-plugins-sandboxing-crash-protection/786463)). Ardour deliberately refuses, arguing in-process is the only way plugins perform predictably ([Ardour: plugins in process](https://ardour.org/plugins-in-process.html)).
- **Recommendation:** for a solo v1, do out-of-process *scanning* + crash blacklist + a structured-exception guard around editor open; skip runtime sandboxing (months of IPC work for marginal personal benefit).

### 1.5 pluginval for testing the host side

✅ [pluginval](https://github.com/Tracktion/pluginval) (Tracktion, GPLv3) is a headless-capable validator: strictness 1–10 (5 = baseline host compatibility), exit code 0/1, designed for CI ([Adding pluginval to CI](https://github.com/Tracktion/pluginval/blob/develop/docs/Adding%20pluginval%20to%20CI.md), [Testing plugins with pluginval](https://github.com/Tracktion/pluginval/blob/develop/docs/Testing%20plugins%20with%20pluginval.md)). Flags seen in docs: `--strictness-level`, `--validate-in-process`, `--output-dir`.

How it helps a **host** developer (⚠️ usage pattern is inference, tool facts are ✅):
1. **Indirectly:** pluginval *is* a JUCE host exercising the same `juce_audio_processors` code paths you use — its test suite (instantiate, process, automate, fuzz parameters, save/restore state, open/close editor, multiple sample rates/block sizes) is a checklist of exactly what your host must survive.
2. **Directly in your CI:** build a small matrix of known-good free plugins (e.g., Airwindows Consolidated VST3, Surge XT, Valhalla SuperMassive, JUCE example plugins) and run a headless smoke test that loads each through *your* engine wrapper: scan → instantiate → process N blocks → save/restore state → unload, asserting no crash/leak. Steal pluginval's test ordering. Because its source is GPLv3 like your codebase, you can lift its test logic directly.
3. Use pluginval itself in CI for any built-in effects you also export as plugins.

---

## 2. CLAP hosting status (as of mid-2026)

- ✅ [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) is for *creating* CLAP plugins from JUCE projects — explicitly not hosting ([discussion #152: CLAP host support](https://github.com/free-audio/clap-juce-extensions/discussions/152)).
- ✅ **JUCE official:** the roadmap ([Q1 2025](https://juce.com/blog/juce-roadmap-update-q1-2025/), [Q3 2025, 2025-11-20](https://juce.com/blog/juce-roadmap-update-q3-2025/)) promises **CLAP *authoring* in JUCE 9** — no commitment to CLAP **hosting** in any published roadmap through the latest available update (Q3 2025). ❓ No Q1/Q2 2026 roadmap post was retrievable this session (forum.juce.com blocked fetch with HTTP 403); long-running FR: [FR: Support CLAP for plugins (host & client)](https://forum.juce.com/t/fr-support-clap-for-plugins-host-client/51860). **Conclusion: as of mid-2026, no shipped official JUCE CLAP hosting.**
- ✅ **Community module:** [jatinchowdhury18/juce_clap_hosting](https://github.com/jatinchowdhury18/juce_clap_hosting) (MIT) implements a CLAP `AudioPluginFormat` — self-described "super-alpha": scanning + stereo audio + basic params work; **missing MIDI, state, GUI, latency extensions**; ~6 commits, stale. Could be a starting point to fork, not a dependency.
- ✅ **clap-wrapper is not a host-side bridge:** [free-audio/clap-wrapper](https://github.com/free-audio/clap-wrapper) builds a VST3/AUv2/standalone binary that loads *a specific CLAP of the same name* — it's deployed by the **plugin developer**, per plugin. It does not let your VST3 host transparently load arbitrary installed CLAPs. (Practical upside ✅: many CLAP-first plugins already ship a clap-wrapper VST3, so your VST3-only host loses little.)
- ✅ Tracktion Engine has no CLAP hosting; its plugin formats are JUCE's.

**Recommendation: host VST3 + AU for v1, punt CLAP.** On macOS, **Audio Unit (AU) is the native, ubiquitous plugin format** — Logic-only and AU-first vendors are common, and JUCE/Tracktion host it for almost no extra work via JUCE's `AudioUnitPluginFormat` (the engine's `te::ExternalPlugin` already covers AU). Enable both VST3 and AU scanning from day one; it roughly doubles your usable plugin coverage on Mac at near-zero cost. (Optionally LV2 too, which JUCE 8 hosts natively — ⚠️ verify quality before relying on it.) Architect your `te::ExternalPlugin` usage so a future CLAP `AudioPluginFormat` (JUCE 9+ or a matured community module) slots in. Revisit CLAP when the JUCE 9 feature set is announced.

---

## 3. Built-in effects plan

### 3.1 What Tracktion Engine ships (verified from [tracktion_PluginManager.cpp](https://github.com/Tracktion/tracktion_engine/blob/master/modules/tracktion_engine/plugins/tracktion_PluginManager.cpp))

✅ Registered built-in plugin types:

| Category | Plugins |
|---|---|
| Mixing/utility | `VolumeAndPanPlugin`, `VCAPlugin`, `LevelMeterPlugin`, `TextPlugin`, `FreezePointPlugin`, `InsertPlugin` |
| Effects | `EqualiserPlugin` (4-band), `ReverbPlugin`, `CompressorPlugin`, `ChorusPlugin`, `DelayPlugin`, `PhaserPlugin`, `PitchShiftPlugin`, `LowPassPlugin` |
| Routing | `PatchBayPlugin`, `AuxSendPlugin`, `AuxReturnPlugin`, `MidiPatchBayPlugin` |
| Instruments/MIDI | `SamplerPlugin`, `FourOscPlugin` (4-osc synth), `MidiModifierPlugin` |
| Legacy/conditional | `ReWirePlugin` (if `TRACKTION_ENABLE_REWIRE`) |

✅ **Bonus:** with `TRACKTION_AIR_WINDOWS=1` the engine registers **200+ Airwindows effects as built-in plugins** (`AirWindowsXXX` types) — an enormous free effects rack already integrated with engine automation/state.

⚠️ Quality assessment (community consensus + these are the original 2003-era Tracktion effects): functional but utilitarian — the EQ is a basic 4-band, the reverb is simple (juce::Reverb-class/freeverb-style), compressor is serviceable. Fine for v1 plumbing; you'll want better-sounding flagship EQ/compressor/reverb eventually. (Tracktion sells its better "DAW Essentials" effects [commercially](https://www.tracktion.com/products/daw-essentials-collection) — those are not in the open engine.)

### 3.2 Open-source DSP to wrap

| Source | License | Provides | Notes |
|---|---|---|---|
| `juce::dsp` (JUCE 8) | AGPLv3/commercial (AGPL OK for your GPLv3 app ⚠️ check compatibility direction — AGPL code can't be relicensed GPLv3; ship app as AGPLv3 or use JUCE 8's license terms) | IIR/SVF filters, FIR, Convolution, `Compressor`, `Limiter`, `Reverb` (freeverb-quality), `Chorus`, `Phaser`, `DelayLine`, oversampling | Already a dependency; quality: solid filters/convolution; basic dynamics/reverb |
| [chowdsp_utils](https://github.com/Chowdhury-DSP/chowdsp_utils) | Per-module: **BSD** (common/music) + **GPLv3** (DSP & GUI) — GPLv3 fine for you | State-variable & higher-order filters (Butterworth/Cheby/Elliptic), EQ bands + `LinearPhaseEQ`, compressor (level detector/gain computer), FDN & Dattorro reverbs, delay lines, pitch shift, ADAA waveshapers, resampling | Highest-quality modern C++ DSP toolkit in JUCE-land; core DSP is C++17 header-style |
| [Airwindows via airwin2rack](https://github.com/baconpaul/airwin2rack) | **MIT** | 350+ effects as a single consolidated static library / registry | Best license, huge breadth; characterful but mostly knob-light designs; or just flip `TRACKTION_AIR_WINDOWS` |
| [sst-effects](https://github.com/surge-synthesizer/sst-effects) (Surge XT effects) | **GPL-3.0** (✅ verified; depends on sst-basic-blocks, sst-filters) | Header-only templates of Surge's effects (reverb, flanger, more being factored out; actively maintained — last update 2026-06) | High-quality, GPL3-compatible with your codebase |
| [DPF](https://github.com/DISTRHO/DPF) / x42 plugins | DPF: **ISC**; x42 & DPF-Plugins: mixed **GPL2/GPL3/LGPL** | x42 EQ/meters etc. are LV2-first | Less convenient: different framework; only worth it via LV2 hosting, not source-wrapping |

### 3.3 Concrete starter set recommendation

Wrap each as a `te::Plugin` subclass (engine handles state/automation/PDC):

| Slot | v1 (ship this) | License | v2 upgrade path |
|---|---|---|---|
| Channel EQ | `te::EqualiserPlugin` (already there) | GPLv3-compatible | chowdsp EQ bands → parametric 6-band + `LinearPhaseEQ` (GPLv3) |
| Filter | `chowdsp::StateVariableFilter` (LP/HP/BP/notch + drive) | GPLv3 | — |
| Compressor | `juce::dsp::Compressor` wrapped with your UI | AGPL/JUCE | chowdsp level-detector/gain-computer (adds modes, program-dependent release) |
| Delay | `te::DelayPlugin` for v1 | GPLv3-compat | chowdsp delay line w/ interpolation + feedback filtering, tempo sync |
| Reverb | `te::ReverbPlugin` (freeverb) for plumbing | GPLv3-compat | chowdsp FDN/Dattorro, or sst-effects Surge Reverb2 (GPL3) |
| Limiter | `juce::dsp::Limiter` on master bus | AGPL/JUCE | chowdsp + true-peak detection |
| Character rack | `TRACKTION_AIR_WINDOWS=1` → 200+ Airwindows built-ins (MIT) | MIT | curate a "best of" menu instead of all 350 |

Everything above is license-compatible with a GPLv3 application (⚠️ one flag: JUCE 8 personal/AGPL terms make your effective app license AGPLv3 unless you hold a JUCE commercial license — decide once, early).

---

## Key hurdles (summary)

1. **Plugin scanning crashes** — worst DAW-killer. Engine solves it: `te::PluginManager::setUsesSeparateProcessForScanning(true)` + multithreaded scan + blacklist. Use from day one.
2. **macOS editor embedding** — `NSView` parenting, focus/keyboard handling, z-order, layer-backing/clipping in deep hierarchies. Mitigate with one top-level window per plugin editor.
3. **Retina / mixed-display** — generally well-behaved on macOS, but still test editors across a 2× built-in + 1× external monitor and dragging between them. Maintain a small test matrix of non-JUCE plugins.
4. **Resizable plugin editors** — host must react to plugin-initiated resizes (`IPlugView::onSize` round-trips); historic JUCE glitches documented.
5. **PDC** — handled automatically by tracktion_graph (`getLatencySeconds()` propagation); test runtime latency *changes* and aux/rack routing.
6. **Plugin state save/restore** — engine serializes plugin state into the Edit; test with state-heavy plugins (samplers) and against pluginval-style double-restore.
7. **Runtime crash protection** — JUCE offers none; Waveform's sandbox is proprietary; Bitwig-style isolation is a big IPC project. Defer; rely on scan-time protection + blacklist.
8. **CLAP** — no official JUCE hosting through Q3 2025 roadmap (JUCE 9 = authoring only); community module is alpha; clap-wrapper is plugin-side. Punt to post-v1.

## Recommendations (action list)

1. Host **VST3 + AU** for v1 via `te::ExternalPlugin` (AU is the macOS-native format and essentially free to host with JUCE); design for adding a CLAP `AudioPluginFormat` later.
2. Enable engine **separate-process scanning** immediately; add a crash blacklist UI.
3. Build a **CI host-smoke-test**: load 5–10 known-good free plugins headlessly through the engine (scan → process → state round-trip); borrow test ordering from pluginval (GPLv3, same as you).
4. Ship engine built-ins + `TRACKTION_AIR_WINDOWS` for instant breadth; plan chowdsp-based EQ/compressor/reverb as the quality tier.
5. Test on a Retina (2×) laptop display + a non-Retina (1×) external monitor with FabFilter/u-he/Vital (and a couple of native AU) demos before calling hosting "done".

---

## Sources

- JUCE forum editor-embedding / scaling: [44170](https://forum.juce.com/t/vst3-plugin-editor-resizing-glitch-issues/44170), [53043](https://forum.juce.com/t/scaling-ui-of-hosted-plugin/53043), [18240](https://forum.juce.com/t/scale-size-issue-with-plugin-window-very-very-small-from-plugin-host-example/18240), [18566](https://forum.juce.com/t/crash-closing-hosted-plugin-editor/18566) (accessed 2026-06-11)
- PDC: [tracktion_engine FEATURES.md](https://github.com/Tracktion/tracktion_engine/blob/develop/FEATURES.md), [JUCE forum 62110](https://forum.juce.com/t/how-is-internal-plugin-latency-applied-in-tracktion-engine/62110), [53709](https://forum.juce.com/t/custom-te-plugin-and-latency/53709), [KVR 551189](https://www.kvraudio.com/forum/viewtopic.php?t=551189), [KVR 518334](https://www.kvraudio.com/forum/viewtopic.php?t=518334)
- Scanning/sandboxing: [tracktion_PluginManager.h](https://github.com/Tracktion/tracktion_engine/blob/develop/modules/tracktion_engine/plugins/tracktion_PluginManager.h), [JUCE AudioPluginHost MainHostWindow.cpp (Superprocess/ChildProcessCoordinator)](https://github.com/juce-framework/JUCE/blob/master/extras/AudioPluginHost/Source/UI/MainHostWindow.cpp), [JUCE issue #997](https://github.com/juce-framework/JUCE/issues/997), [forum 58485](https://forum.juce.com/t/vst3-plugin-scanning-crash-protection/58485), [19006](https://forum.juce.com/t/plugin-scanning-process-always-crashes-the-application/19006), [49487](https://forum.juce.com/t/coping-with-plug-in-load-scan-failures-e-g-exc-bad-access/49487), [Waveform Pro features](https://www.tracktion.com/products/waveform-pro-features), [KVR 591961](https://www.kvraudio.com/forum/viewtopic.php?t=591961), [Ardour: plugins in process](https://ardour.org/plugins-in-process.html), [Steinberg forum sandboxing request](https://forums.steinberg.net/t/solution-to-crashes-by-plugins-sandboxing-crash-protection/786463), [forum 44878](https://forum.juce.com/t/possible-tracktion-waveform-11-5-sandbox-bug-with-non-automatable-audio-parameters-and-vst3/44878)
- pluginval: [repo](https://github.com/Tracktion/pluginval), [CI doc](https://github.com/Tracktion/pluginval/blob/develop/docs/Adding%20pluginval%20to%20CI.md), [Verified by pluginval](https://www.tracktion.com/develop/pluginval)
- CLAP: [JUCE roadmap Q1 2025](https://juce.com/blog/juce-roadmap-update-q1-2025/), [Q3 2025](https://juce.com/blog/juce-roadmap-update-q3-2025/), [FR thread 51860](https://forum.juce.com/t/fr-support-clap-for-plugins-host-client/51860), [juce_clap_hosting](https://github.com/jatinchowdhury18/juce_clap_hosting), [clap-wrapper](https://github.com/free-audio/clap-wrapper), [clap-juce-extensions #152](https://github.com/free-audio/clap-juce-extensions/discussions/152)
- Effects/DSP: [tracktion_PluginManager.cpp](https://github.com/Tracktion/tracktion_engine/blob/master/modules/tracktion_engine/plugins/tracktion_PluginManager.cpp), [chowdsp_utils](https://github.com/Chowdhury-DSP/chowdsp_utils), [airwin2rack](https://github.com/baconpaul/airwin2rack), [Airwindows Consolidated](https://www.airwindows.com/consolidated/), [sst-effects](https://github.com/surge-synthesizer/sst-effects), [DPF LICENSING](https://github.com/DISTRHO/DPF/blob/main/LICENSING.md), [DAW Essentials (commercial, not in engine)](https://www.tracktion.com/products/daw-essentials-collection)
