# DAW Development To-Do List

Master plan for building the DAW (Tracktion Engine + JUCE 8, C++20, CMake, macOS, GPLv3-compatible) with integrated online sound discovery. Distilled from [Research/](../Research/README.md) — see [07-deliverables.md](../Research/07-deliverables.md) for the stack rationale and risk register.

**Task ownership legend:**
- **[You]** — manual setup, downloads, accounts, listening tests, decisions
- **[CC]** — development task for a Claude Code session
- **[You+CC]** — collaborative (you drive, Claude assists)

Every phase ends with the app in a usable state and with automated checks Claude Code can run without human listening.

---

## Part A — One-time environment setup [You]

### A1. Machine / toolchain

- [x] Install **Xcode** (full app from App Store, not just CLT — needed for the macOS SDK) and run `xcode-select --install`; accept the license (`sudo xcodebuild -license accept`) — Xcode 26.4 verified in Spike 2; **gotcha:** the installed CLT ships a *newer* SDK (macOS 27) than Xcode's clang understands, so builds must pin `CMAKE_OSX_SYSROOT` to Xcode's SDK (see decisions)
- [x] Install **Homebrew** (https://brew.sh) — 6.0.0 verified
- [x] `brew install cmake ninja ccache git-lfs llvm` — verified in Spike 2: CMake 4.3.3, Ninja 1.13.2, ccache 4.13.6, Homebrew LLVM present
  - Homebrew **LLVM ≥ 20** is required for RealtimeSanitizer (`-fsanitize=realtime`) — Apple's bundled clang doesn't ship the realtime runtime. Normal dev builds still use AppleClang.
- [x] `git lfs install` (golden-render WAV fixtures will live in LFS) — global lfs filters verified present
- [x] Install **Python 3.11+** with a venv for prototyping and the later embedding sidecar: `~/venvs/daw` (Python 3.12); deps finalized in Spike 1 → `tools/requirements-spike1.txt` (note: torch/torchvision/torchaudio must be installed explicitly; `freesound-api` not needed)
- [ ] Claude Code: install the official **clangd LSP plugin** (https://claude.com/plugins/clangd-lsp) — clangd comes with the Homebrew LLVM you just installed; the plugin needs `compile_commands.json`, which our CMake preset will export

### A2. Accounts & credentials

- [ ] Create the **GitHub repository** and push this repo (private is fine; GPL obligations only trigger on distribution)
- [ ] Enable **GitHub Actions**; be aware macOS runners bill at a **10× minute multiplier** — the CI plan keeps the high-volume loop on Linux if minutes get tight
- [x] Register a **Freesound** account, then create API credentials at https://freesound.org/apiv2/apply :
  - API key (token auth) — done, verified working in Spike 1 (credentials in vault-root `.env`; the client secret is the token-auth key)
  - OAuth2 client (`client_id` + `client_secret` + redirect URL) — done; redirect registered as Freesound's hosted paste-the-code page (`/home/app_permissions/permission_granted/`), so Phase 4 uses that flow, not a loopback listener
  - Note: rate limits are 60 req/min, 2,000 req/day **per key**; the app will require each user to bring their own key (open-source apps can't embed a secret)

### A3. Test assets (downloads)

- [ ] Download free plugins for hosting tests (VST3 **and** AU where available): **Surge XT**, **Valhalla Supermassive**, **Airwindows Consolidated**, **Vital**, plus one **u-he** demo and one **FabFilter** demo (important non-JUCE coverage). Apple's built-in AUs (AUDelay etc.) are already on the system.
- [ ] Collect a small folder of test samples — a few WAV, FLAC, and MP3 files of varying sample rates/channel counts — for import and fixture material
- [ ] Optional but valuable: access to a **non-Retina external display** for mixed-DPI plugin-window testing (Risk #5)

### A4. Decisions (make once, early)

- [ ] **License**: project code is **GPL-3.0-or-later**; distributed builds incorporate JUCE 8 under **AGPLv3** (one-way compatible — state it correctly in README/COPYING). Confirmed by the licensing audit: no component blocks this.
- [ ] **App name** — affects bundle identifier, CMake target names, project-file extension branding. Pick before Phase 0.
- [ ] Manual license verifications the audit couldn't fetch programmatically (none block GPL path, just confirm): juce.com EULA page, engine.tracktion.com/agreement, HF license tags for `laion/larger_clap_*`

### A5. Learning (optional, recommended)

- [ ] Watch **"Why You Shouldn't Write a DAW"** — Dave Rowland, ADC23 (the hurdles, from the engine's lead dev)
- [ ] Watch **"Introducing Tracktion Graph"** — ADC20 (how the playback graph works)
- [ ] Skim the engine's 4 tutorials + the 15 DemoRunner demo sources (`examples/DemoRunner/demos/`) — these demos *are* the documentation

---

## Part B — De-risking spikes (before heavy C++)

### Spike 1 — Freesound semantic-search validation [You+CC] · ~1 day · **decides Phase 4's headline feature**

The plan hinges on an undocumented trick: passing a locally computed CLAP **text** embedding as `similar_to=[...512 floats]&similarity_space=laion_clap` so Freesound's server does semantic nearest-neighbor search (Risk #3).

- [x] Python script: embed text queries with LAION-CLAP checkpoint **`630k-audioset-fusion-best`** (= HF `laion/clap-htsat-fused` — must match Freesound's server-side model exactly) — `tools/spike1_freesound_semantic.py`
- [x] Call `GET /apiv2/search/?similar_to=[vec]&similarity_space=laion_clap` with the API key; judge result quality on ~10 varied queries ("vinyl crackle loop", "punchy kick", "rain on tent"...)
- [x] Also test while in there: `query` + `similar_to` interaction (undocumented precedence); license filter composition; `GET /sounds/<id>/analysis/` vector retrieval; what license string a pre-2011 Sampling+ sound returns; live docs vs repo-master docs drift
- [x] **Record the verdict in `dev/decisions.md`**: **GO** — semantic path works live; see the 2026-06-11 Spike 1 entry for quality notes and binding API mechanics (GET-only, 4094-char request line, `query` ignored with `similar_to`, deed-URL license strings)

### Spike 2 — Engine builds on this machine [CC] · ~½ day

- [x] Clone `Tracktion/tracktion_engine` at `develop`, **pin a known-good SHA** (last activity Feb 2026), `git submodule update --init --recursive` (JUCE lives at `modules/juce`) — clone lives at `~/src/tracktion_engine`, pinned `2877b621` (engine 3.2.0) + JUCE `7c89e11f` (8.0.12); note: JUCE submodule URL is SSH, needs HTTPS rewrite (see decisions)
- [x] Build **DemoRunner** with CMake + Ninja; run PlaybackDemo; confirm audio out through CoreAudio — built & launches cleanly; engine TestRunner also built & run headless (52/53 pass). **[You] remaining:** open the running DemoRunner, click PlaybackDemo, confirm you hear audio
- [x] Record in `dev/decisions.md`: engine SHA, JUCE submodule SHA, build time cold/warm, any patches needed — see the 2026-06-11 Spike 2 entry

---

## Phase 0 — Skeleton (build system, test harness, CI, first sound)

Goal: a clean repo where `cmake --preset dev && cmake --build --preset dev && ctest --preset dev` works first try, in CI and locally, with a sine wave provably rendered.

### Dependencies & build [CC]

- [x] Add `libs/tracktion_engine` as a **pinned git submodule** (recursive — brings JUCE). Never edit submodule contents; local patches go in `patches/` with an apply script.
- [x] Wire **Rubber Band** time-stretching: vendor `breakfastquay/rubberband` (own submodule under `libs/`), copy/symlink into the engine's expected `3rd_party/rubberband` path at configure time, set `TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND=1` + `TRACKTION_BUILD_RUBBERBAND=1` (single-file build). Keep SoundTouch enabled as the A/B fallback.
- [x] Root `CMakeLists.txt` + **`CMakePresets.json`** with presets: `dev` (Ninja, Debug, ccache, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, arm64), `release` (universal2), `rtsan` (Homebrew LLVM clang, `-fsanitize=realtime`), `asan` (`-fsanitize=address,undefined`) — per the skeleton in [Research/05](../Research/05-claude-code-dev-workflow.md) §2
- [x] Three targets: **`daw_core`** static lib (engine glue, zero GUI includes), **`daw_tests`** console test runner (links `daw_core` only — DSP iteration never relinks the GUI app), **`EZStudio`** GUI shell
- [x] `target_precompile_headers` (JUCE + Tracktion umbrella headers) on app + core; **no unity builds**
- [x] Test deps: **Catch2** + **melatonin_test_helpers** (AudioBlock matchers, sparkline output Claude can read)
- [x] Build-speed check: measure and record incremental rebuild of `daw_tests` after touching one `.cpp` — **target < 30 s** (measured ~6 s warm; see Phase 0 closeout entry in `dev/decisions.md`)

### Real-time safety foundation [CC]

- [x] `src/rt/`: the **single facade** for all cross-thread traffic — moodycamel `ReaderWriterQueue` wrapper (`try_enqueue` only on the RT side), `RT_NONBLOCKING` macro → `[[clang::nonblocking]]`, debug-build allocation guard (`RT_CHECK`)
- [x] Write `docs/realtime-rules.md` enforcement into CLAUDE.md + a grep tripwire in CI (`rg "std::mutex|malloc|\bnew\b" src/engine/dsp/`)

### First sound + first render test [CC, then You]

- [x] [CC] Headless `te::Engine` + `Renderer::renderToFile`: programmatic Edit with a 440 Hz sine → temp WAV → assert RMS level and dominant FFT bin within tolerance. This is the prototype for all future render tests.
- [x] [CC] `EZStudio` plays a sine through the default CoreAudio output device
- [ ] [You] Listen: confirm sound comes out of your interface

### CI [CC]

- [x] `.github/workflows/build-and-test.yml`: macOS (Apple-silicon runner) primary lane + cheap Linux cross-check lane; ccache via `hendrikmuhs/ccache-action`; recursive submodule checkout; `ctest` headless; failed-render WAVs uploaded as artifacts
- [x] Separate **rtsan** job (macOS, Homebrew LLVM): render tests under `-fsanitize=realtime`
- [x] Periodic (weekly) **TSan + ASan** lane over multithreaded engine tests

### Claude Code harness [CC]

- [x] Finalize root `CLAUDE.md` with the **exact** build/test commands (acceptance test: fresh session told "run the tests" succeeds first try)
- [x] Per-directory `CLAUDE.md` for `src/engine/`, `src/ui/`, `tests/`
- [x] Hooks: PostToolUse `clang-format -i` on edited C++ files; Stop hook running build + ctest as a turn gate
- [x] Skills: `render-test` (how to author a golden render test), `tracktion-api` (Edit/Renderer/TransportControl cheat-sheet), `rt-review` (audio-thread diff checklist)

### Phase 0 acceptance

- [x] Fresh clone + documented commands → green build + tests, locally and in CI
- [x] Sine render test passes headlessly; ~~sine audible through speakers~~ (audibility = the [You] listen item above)
- [x] Incremental `daw_tests` rebuild < 30 s

---

## Phase 1 — Timeline (arrangement MVP)

Goal: import audio files, arrange/trim/move clips on a timeline, play, undo, export WAV. All UI is hand-built — the engine provides none (this is the documented solo-DAW killer; scope discipline applies).

### Tasks [CC]

- [ ] **Project lifecycle**: new/open/save `.tracktionedit`, recent-files list, dirty-state tracking
- [ ] **Transport bar**: play/stop/loop, time readout (bars/beats *and* min:sec), loop-region UI
- [ ] **Timeline view**: track lanes, time ruler, zoom/scroll with correct pixel↔`TimePosition` mapping (tempo-curve aware), playhead drawing + auto-follow
- [ ] **Audio import**: drag-and-drop from Finder (`FileDragAndDropTarget`) onto tracks/empty area; WAV/FLAC/MP3 via JUCE readers + macOS CoreAudioFormat
- [ ] **Waveform rendering**: `SmartThumbnail`-backed; cache rendered strips as tiled images (stay under CoreGraphics image-size ceilings), draw only the visible range, invalidate on zoom. Naive drawing won't survive DAW scale — budget profiling time here.
- [ ] **Clip interactions**: select (single/marquee), drag-move with snapping, trim handles, fade handles, copy-drag, delete, right-click menu
- [ ] **Undo/redo**: `UndoManager` transaction per UI gesture (`beginNewTransaction` at gesture start); UI updates by listening to ValueTree changes, never by assuming its own action was the only mutation
- [ ] **Export**: render the edit to WAV via `Renderer`
- [ ] Keep a runtime **renderer toggle** (CoreGraphics / software) for A/B profiling of UI performance

### Checks [CC]

- [ ] Tolerance-based golden render tests: clip placement, trim offsets, gain — peak error < −96 dBFS vs goldens (goldens in Git LFS; regen only via explicit `daw_regen_goldens` target)
- [ ] Null test: render → undo+redo round-trip → re-render → diff ≈ silence
- [ ] `createComponentSnapshot` PNG regression for the timeline (pinned CI OS, per-pixel tolerance for fonts)

### Phase 1 acceptance [You]

- [ ] Arrange a few loops into a short track, trim/fade/move them, export a WAV, listen — note every friction point in `dev/feedback.md`

---

## Phase 2 — Mixer, built-in FX, VST3/AU hosting

Goal: a mixer with metering, day-one usable effects, and stable third-party plugin hosting.

### Tasks [CC]

- [ ] **Mixer view**: channel strips, faders/pans bound to engine `AutomatableParameter`s, meters fed by `LevelMeasurer`, mute/solo, master strip
- [ ] **Built-in FX**: enable Tracktion built-ins (4-band EQ, compressor, delay, reverb, chorus, phaser…) + `TRACKTION_AIR_WINDOWS=1` (200+ Airwindows effects, MIT); plugin-slot UI on strips; a *curated* Airwindows menu, not all 350
- [ ] **VST3 + AU hosting** via `te::ExternalPlugin` (AU is the macOS-native format — near-free to add, doubles plugin coverage). Architect so a future CLAP `AudioPluginFormat` slots in (punt CLAP itself — no viable JUCE hosting path as of mid-2026).
- [ ] **Out-of-process plugin scanning from day one**: `te::PluginManager::setUsesSeparateProcessForScanning(true)`, child-process scan entry point in app startup (`startChildProcessPluginScan`), multithreaded scan, crash **blacklist** + UI to manage it
- [ ] **Plugin windows**: one top-level `DocumentWindow` per editor (the Waveform pattern — avoids NSView z-order/focus/clipping pain); handle plugin-initiated resizes
- [ ] Skip runtime plugin sandboxing (Waveform's is proprietary; months of IPC for marginal solo benefit)

### Checks [CC]

- [ ] Render-with-EQ golden tests; FFT assertion ("low-pass at 1 kHz attenuates 4 kHz by ≥ 24 dB")
- [ ] **PDC null test**: latency-inducing plugin in one path vs dry path, compensated diff ≈ silence; also test runtime latency *changes* (lookahead toggle → graph rebuild)
- [ ] **Host smoke test in CI**: headless scan → instantiate → process N blocks → save/restore state → unload across the free-plugin matrix; borrow test ordering from pluginval (GPLv3, same license as us)
- [ ] Plugin state round-trip with a state-heavy plugin (sampler)

### Phase 2 acceptance [You]

- [ ] Mix a small session with built-in EQ/comp + 2–3 third-party plugins (include FabFilter/u-he/Vital demos and a native AU)
- [ ] Manual display-matrix test: plugin editors on Retina laptop screen + non-Retina external, drag windows between them

---

## Phase 3 — Freesound integration (token auth) · **the differentiator begins**

Goal: search Freesound inside the DAW, audition previews, drag results onto the timeline with license metadata intact.

### Tasks [CC unless marked]

- [ ] [You] Paste your Freesound API key into the app's settings (and document the bring-your-own-key onboarding for future users)
- [ ] Fork/vendor **`MTG/freesound-juce`** as the client skeleton; modernize to the post-Nov-2025 `GET /apiv2/search/` endpoint (old `/search/text/` is deprecated)
- [ ] **Search UI**: query box; filter controls for duration, type, samplerate, license, `loopable`, `single_event`, `bpm`, `tonality` (Freesound precomputes the musical descriptors a DAW wants); sort options; paginated results
- [ ] Request shaping: single-request results via `fields=id,name,username,license,duration,previews,images,tags,filesize,type,samplerate`; `group_by_pack`; handle HTTP 429 gracefully (rate limits: 60/min, 2,000/day)
- [ ] **Results list**: Freesound's pre-rendered waveform PNGs, duration, author, **license badge** (color-coded); default filter `license:("Attribution" OR "Creative Commons 0")` — NC excluded by default, unknown/legacy licenses (Sampling+) hidden or warned
- [ ] **Preview audition**: stream HQ-ogg preview (token auth suffices) with play/stop in the result row
- [ ] **Drag-to-timeline**: import the HQ-ogg preview as a clip; persist `{author, sound URL, sound name, license, md5}` into clip + project file; mark clip as *lossy placeholder* (upgrade flow comes in Phase 4)
- [ ] **Attribution machinery**: per-project credits list, exportable `credits.txt`; **export-time warning** if any NC/unknown-licensed or placeholder asset is in the session
- [ ] ToS compliance guardrails in code: no catalog mirroring, cache only user-driven content

### Checks [CC]

- [ ] API client tested against **recorded fixtures** (no live API calls in CI)
- [ ] Project-file round-trip preserves license metadata; credits exporter unit test
- [ ] NC-warning triggers correctly (unit test)

### Phase 3 acceptance [You]

- [ ] Find, audition, and place 5 sounds in a project without leaving the DAW; export and confirm the credits file is correct

---

## Phase 4 — Semantic search, originals, local library

Goal: natural-language sound search across Freesound *and* your own sample folder; original-quality upgrades.

### Tasks [CC unless marked]

- [ ] **In-process ONNX text encoder**: `laion/clap-htsat-fused` text tower via ONNX Runtime (CPU EP, MIT) + RoBERTa tokenizer (HF `tokenizers` C bindings). Pre-exported `Xenova/clap-htsat-unfused` ONNX exists as a reference. **Parity gate: output matches Python reference ≤ 1e-4.**
- [ ] **Semantic path** (feature-flagged, per Spike 1 verdict): text vector → `similar_to=[...]&similarity_space=laion_clap`
- [ ] **Hybrid query parser**: bpm/key/duration/"loop"/"one-shot" terms → symbolic Freesound filters (`bpm`, `tonality`, `loopable`, `single_event`); only the timbral residue goes to CLAP (CLAP is blind to BPM/key and weak on <1 s one-shots — this hybrid beats pure-CLAP)
- [ ] **"More like this"** button on every result (`similar_to=<id>` — documented, zero risk)
- [ ] **OAuth2 flow**: browser round-trip (loopback redirect if Freesound allows, else the official paste-the-code fallback); access/refresh tokens in the **macOS Keychain**; auto-refresh (24 h expiry)
- [ ] **Upgrade-to-original flow**: replace placeholder preview with the original download *in place*, sample-accurate re-alignment via waveform cross-correlation (mp3/ogg encoder delay shifts a few ms)
- [ ] **Local sample library**: SQLite DB (paths, tags, license, source URL, md5) + **sqlite-vec** for 512-d embeddings (pin the version; wrap vector access behind one interface — escape hatch: usearch)
- [ ] **Python sidecar** (FastAPI + `laion_clap`, loopback-only): background-embeds the user's sample folder with the reference audio tower — avoids the broken STFT/mel ONNX export entirely; search must keep working when the sidecar is down
- [ ] **Unified results**: one search box queries Freesound + local library with the same text vector; merged, deduped results
- [ ] **Dedup**: Freesound `md5` + local file md5 (exact), Chromaprint (loops/recordings), embedding cosine > 0.98 (one-shots)
- [ ] Respect the `ai_preference` field if anything beyond search is ever done with downloaded data

### Checks [CC]

- [ ] Text-encoder parity ≤ 1e-4 vs Python reference (CI fixture)
- [ ] KNN retrieval over a fixture library: known query → known top-k
- [ ] Dedup unit tests (exact, near-dupe, one-shot cosine)
- [ ] OAuth token refresh logic unit-tested against a mock

### Phase 4 acceptance [You]

- [ ] Type "dusty vinyl crackle loop 90 bpm" → relevant results from both Freesound and your own library; upgrade one placeholder to original quality and confirm alignment by ear

---

## Phase 5 — Recording, automation UI, polish

### Tasks [CC]

- [ ] **Audio recording**: arm/input-select per track, count-in, punch in/out, input monitoring (engine handles the capture path; recent upstream commits hardened it)
- [ ] **MIDI recording** incl. retrospective record (engine feature)
- [ ] **Automation lanes**: curve display + editing on tracks (engine stores bezier curves; read/write/touch/latch modes exist since v3.2)
- [ ] **Keyboard-driven workflow**: `ApplicationCommandManager` shortcut system, settings UI
- [ ] Performance pass: profile CoreGraphics repaints (meters, scrolling waveforms); A/B the renderer toggle; consider the Metal layer experiment only if profiling demands it
- [ ] Settings: audio device/buffer selection, plugin paths, Freesound credentials, theme

### Checks [CC]

- [ ] Record-from-virtual-device render test
- [ ] Automation golden test: gain ramp → expected curve in rendered output within tolerance
- [ ] UI snapshot regressions extended to mixer + automation lanes

### Phase 5 acceptance [You]

- [ ] Record a take over imported loops, automate a filter sweep, mix, export — a full song workflow end-to-end

---

## Backlog (post-v1, revisit deliberately)

- **Piano roll / deep MIDI editing** — one of the largest single UI builds in a DAW; scope it as its own project
- **CLAP hosting** — revisit when JUCE 9 ships (authoring promised; hosting unannounced as of mid-2026)
- **Quality FX tier** — chowdsp_utils EQ/compressor/reverb, sst-effects (Surge) reverb to replace the utilitarian built-ins
- **Clip launcher / step sequencer** — engine supports both (ClipLauncherDemo, StepSequencerDemo)
- **Key/BPM-synced preview conform** (Splice/Loopcloud-style: audition in project key/tempo, drag delivers conformed audio)
- **Additional sound sources** — Openverse (normalized licenses, one API for Freesound+Jamendo+Wikimedia), Internet Archive with hard license filtering; **never** Splice/Loopcloud/Zapsplat/Soundsnap (ToS-forbidden); BBC link-out only (RemArc license incompatible)
- **Runtime plugin sandboxing**, DAWproject import, Ableton Link, ARA2 surfacing

---

## Ongoing practices (every phase)

- **Monitor `tracktion_engine` upstream** — 3-person team, no external PRs, quiet since Feb 2026. If silence passes ~a year, re-evaluate (worst case: keep building on the frozen GPL snapshot).
- **Never edit submodule contents**; patches live in `patches/` and are documented. Bump submodules deliberately: regen goldens via `daw_regen_goldens`, review render diffs.
- **Every DSP/engine change ships with or updates a render test.** Claude must run tests and paste output before claiming done.
- **RT discipline**: all cross-thread traffic through `src/rt/`; RTSan lane stays green; reviewer-subagent checklist on audio-path diffs.
- **License bookkeeping**: update `THIRD_PARTY_LICENSES.md` with every new dependency; keep attribution plumbing intact.
- Keep `CLAUDE.md` files current and short; record architecture decisions in `dev/decisions.md`.
