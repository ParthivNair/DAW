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

## 2026-06-11 — Phase 0 Chunk 2: build skeleton (CMake + presets + targets)

App identity (locked): name **EZStudio**, bundle id `com.parthivnair.ezstudio`,
company "Parthiv Nair". Targets: **EZStudio** (GUI app, `juce_add_gui_app`),
**daw_core** (STATIC engine glue, my code, ZERO GUI includes), **daw_tests** (Catch2
console runner, links `daw_core` only). Project name `daw`, C++20, macOS arm64.

**CMake structure decision — the engine compiles into its own PCH-free `daw_engine`
lib, not into `daw_core`.** JUCE modules amalgamate their sources into whatever target
links the module *interface* targets, and those amalgamation TUs `#error "Incorrect
use of JUCE cpp file"` if ANY prefix header (a force-included PCH) precedes them — and
they mix C++/Objective-C++, which one PCH can't satisfy. So an internal `daw_engine`
STATIC lib links the tracktion/JUCE modules **PRIVATE** (sources compile there once,
no PCH) and re-exports them to consumers via `$<LINK_ONLY:tracktion::...>` plus the
interface include dirs + `TRACKTION_*`/JUCE defines. Result: `daw_core`/`EZStudio`/
`daw_tests` link the engine's symbols + frameworks WITHOUT recompiling module sources,
which is exactly what lets `daw_core`/`EZStudio` carry a PCH. `target_precompile_headers`
uses `__cplusplus`-guarded wrapper headers (`src/engine/DawCorePCH.h`, `src/ui/EZStudioPCH.h`)
so the C-language PCH CMake also emits (project enables C for JUCE's C TUs) is a no-op.
No unity builds.

**Rubber Band** wired per Spike 2: configure-time symlink (copy fallback) of
`libs/rubberband` → `libs/tracktion_engine/modules/3rd_party/rubberband`; defines
`TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND=1` + `TRACKTION_BUILD_RUBBERBAND=1` (engine's
`tracktion_engine.cpp` then `#include`s `<rubberband/single/RubberBandSingle.cpp>`).
SoundTouch stays on as the A/B fallback. `ignore = untracked` on the `libs/tracktion_engine`
submodule entry in our `.gitmodules` keeps the link out of `git status`.

Every preset pins `CMAKE_OSX_SYSROOT=…/Xcode.app/…/MacOSX.sdk` (SDK skew, Spike 2);
Ninja only; `dev` uses ccache + `CMAKE_EXPORT_COMPILE_COMMANDS`; `rtsan` uses Homebrew
LLVM clang 22 (`/opt/homebrew/opt/llvm/bin/clang++`) + `-fsanitize=realtime`; `asan` uses
`-fsanitize=address,undefined`.

**Measured (M1 Pro, this machine):**
- Cold configure (warm build dir): ~4 s; clean configure incl. juceaide + RB link: ~11 s.
- Cold build, empty ccache, `dev` all targets: **50 s** (158/177 ccache misses).
- Clean configure+build+test, warm ccache: 4 s + 11 s + 1.2 s; `ctest --preset dev` 3/3 green.
- **Incremental rebuild of `daw_tests` after touching `src/engine/EngineInfo.cpp`: 2.8 s**
  (well under the 30 s AI-iteration budget — the 300 MB `daw_engine.a` never recompiles).
- `rtsan`: cold build of `daw_tests` ~46 s (Homebrew clang, no ccache hits); `ctest --preset
  rtsan` 3/3 green with `-fsanitize=realtime` intact.
- `asan`: `daw_tests` builds + runs 3/3 green under `-fsanitize=address,undefined`.
- `git status`: engine submodule NOT dirtied by the RB link.

## 2026-06-11 — Phase 0 Chunk 4: first sound + first render test

First-sound milestone and the prototype render test landed. The deterministic source
is the engine's **`ToneGeneratorPlugin`** (sine osc, `producesAudioWhenNoAudioInput()`):
no clip/file/tempo/time-stretch in the path, so the render is bit-stable and the expected
level is trivial. Shared edit builder `src/engine/SineToneEdit.cpp` is used by both the
render test and the GUI.

**Gotcha (binds future plugin work):** `ToneGeneratorPlugin` is **NOT** one of the
engine's default built-in types — `PluginManager::initialise()` omits it — so
`PluginCache::createNewPlugin("toneGenerator", {})` returns null until you call
`engine.getPluginManager().createBuiltInType<ToneGeneratorPlugin>()` first
(idempotent: `registerBuiltInType` skips an already-present type). Also: setting a
plugin's `CachedValue` (`tone->frequency = …`) does **not** refresh the
`AutomatableParameter` the DSP reads in `applyToBuffer()`; you must
`updateFromAttachedValue()` on each parameter (mirrors `restorePluginStateFromValueTree`),
otherwise the render comes out at the plugin defaults (220 Hz, level 1.0).

**Headless render gotcha:** `Renderer::renderToFile(taskDescription, params)` routes through
`UIBehaviour::runTaskWithProgressBar`, which never returns under a headless engine with no
message loop (it hangs). The render helper instead drives a `Renderer::RenderTask` inline
(`while (task.runJob() == jobNeedsRunningAgain) {}`) — the same thing as Renderer's own
`useThread=false` path. Tests that construct an `Engine` also need a process-wide
`juce::ScopedJuceInitialiser_GUI` (a Catch2 listener in `tests/TestMain.cpp`), else JUCE's
global singletons read as leaked at exit.

**Render-test numbers** (`tests/render/SineRenderTest.cpp`, tag `[render]`, 48 kHz, 1.5 s,
440 Hz sine at level 0.5): measured RMS **-9.031 dBFS** (expected -9.03 ±0.5; 0.5/√2),
dominant FFT bin 150 → **439.45 Hz** (expected 440, bin resolution ±2.93 Hz), finite +
not silent. `ctest --preset dev` and `--preset rtsan` both **10/10 green** (9 old + render);
the render test is ~1.5 s and clean under `-fsanitize=realtime` (offline render is the
designed RTSan workload). EZStudio launches, opens the default CoreAudio output
("Output 1 + 2 @ 44100 Hz"), and auto-plays the looped tone (Play/Stop toggle).

**EditClip recheck — now PASSES (Spike 2 follow-up resolved).** Built the engine's own
TestRunner from our pinned submodule (Debug, scratch dir `/tmp/te-testrunner-build`) with
Rubber Band vendored at `modules/3rd_party/rubberband` (CMake logs "Found rubberband,
enabling"). Headless run: **`[doctest] Status: SUCCESS!` — 54/54 cases, 1027/1027
assertions, 0 failed**; `tracktion_EditClip.test.cpp` "EditClip" case passes (3.12 s, no
assertion failures). This confirms the Spike 2 hypothesis: the prior null-test failure
(nested-edit render RMS diff ≈0.692 vs ≈0 expected) was SoundTouch's processing offset
breaking sample-aligned cancellation; Rubber Band fixes it. Nested-edit renders can now be
trusted. (Run was Debug with all benchmarks enabled, hence ~8 min; SHAs unchanged.)

## 2026-06-15 — Phase 0 closeout: skeleton complete, CI green

Phase 0 landed in seven verified chunks on branch `claude/goofy-sammet-7d6fd2` (one
commit each, all gated independently before commit):

1. Vendored pinned submodules under `libs/` + `tools/bootstrap.sh` (JUCE SSH→HTTPS fix) +
   `THIRD_PARTY_LICENSES.md`.
2. CMake skeleton: `dev`/`release`/`rtsan`/`asan` presets (all pin the Xcode SDK sysroot),
   targets `daw_core`/`daw_tests`/`EZStudio`, internal PCH-free `daw_engine` lib, Rubber
   Band wiring.
3. `src/rt/` facade: `RT_NONBLOCKING`, SPSC queue wrapper (producer/consumer views,
   `try_*` only), `RT_CHECK` allocation guard; `tools/rt-tripwire.sh`.
4. First sound + prototype render test (`tests/render/`, `[render]`); EditClip null test now
   passes with Rubber Band (Spike 2 hypothesis confirmed — see Chunk 4 entry above).
5. CI: `build-and-test.yml` (macOS dev + Linux dev + macOS rtsan) + `sanitizers-weekly.yml`
   (ASan/UBSan + TSan); added a `tsan` preset.
6. Claude harness: `.clang-format`, PostToolUse clang-format + Stop build/ctest gate hooks,
   per-dir `CLAUDE.md`, skills `render-test`/`tracktion-api`/`rt-review`.

**Acceptance (all green):**
- **Fresh clone** of the branch into a temp dir → `tools/bootstrap.sh` → `cmake --preset dev`
  → build → `ctest --preset dev`: **10/10 first try**.
- **CI on GitHub Actions** (chunk-5 push): macOS (dev), Linux (dev), macOS (rtsan) all
  **success**. Cold macOS lanes ~4–5 min; Linux cross-check green.
- **Incremental `daw_tests` rebuild** after touching one `.cpp`: **~6 s** warm (budget < 30 s).
- `ctest --preset rtsan` and `--preset tsan` both 10/10 locally; `asan` runs clean.

**No engine patches needed** (`patches/` stays empty). Remaining manual item: **[You]**
launch EZStudio and confirm the 440 Hz sine is audible (the only Phase 0 box that needs a
human). Phase 1 (timeline MVP) is next per `dev/TODO.md`.

## 2026-06-15 — Live-playback gotchas (post-acceptance fix; sine confirmed audible)

The first-sound app was silent at the acceptance listen check; two engine gotchas the
offline render test could not catch (it renders via `RenderTask`, never the live transport):

1. **`Edit::EditRole::forRendering` disables device output.** It sets `playDisabled`
   (`shouldPlay()` → false), so no `EditPlaybackContext` is created — the render path works
   but live playback is silent. `buildSineToneEdit` now takes a `daw::EditPurpose`
   (`livePlayback` → `forEditing`, `offlineRender` → `forRendering`); the GUI session uses
   live playback, the render test opts into offline. **Use `forEditing` for anything that
   plays through a device.**
2. **`ToneGeneratorPlugin` emits whenever the graph processes it**, independent of the
   playhead, and the engine keeps the playback graph live when the transport is merely
   stopped (monitoring) — so `transport.stop(false, false)` left the tone sounding. The
   demo's stop now passes `clearDevices=true` to tear the graph down; `play()` re-allocates
   via `ensureContextAllocated()`. (Tone generator = continuous source; not representative of
   clip playback, which follows the playhead normally.)

Fix verified: 10/10 ctest still green, render test unchanged, Play/Stop audibly correct.
This closes the last Phase 0 box — **Phase 0 is fully complete.**

## 2026-06-15 — Phase 1 Chunk 1: ArrangementEdit + audio-file import

Phase 1 (timeline MVP) starts. Architecture mirrors Phase 0's winning split: the
GUI-free arrangement *model* lives in `daw_core` (`src/engine/`) and is render-tested
headlessly; the GUI shell (Phase-1 later chunks) just drives it. New this chunk:

- **`EditPurpose` extracted** from `SineToneEdit.h` into `src/engine/EditPurpose.h` (the
  `livePlayback`/`offlineRender` enum) so the arrangement model + UI share it without
  coupling to the sine demo. The Edit-builder `.cpp`s map it to `Edit::EditRole` locally,
  so the header still pulls in zero tracktion types.
- **`buildArrangementEdit(engine, numAudioTracks, purpose)`** — multi-track blank Edit via
  `createSingleTrackEdit` + `ensureNumberOfAudioTracks`, master + per-track volume
  neutralised to unity (only clip-level edits show up in a render).
- **`importAudioFileAsClip(track, file, startSecs, deleteExisting)`** — validates
  `te::AudioFile(engine,file).isValid()` then `track.insertWaveClip(name, file,
  ClipPosition{{start, len}, {}}, deleteExisting)`. The **member** `insertWaveClip` taking a
  `juce::File` (not the `ClipOwner` free function, not a `ProjectItemID`) is the demo form.
  `TimePosition`/`TimeDuration` live in `tracktion::` (core), NOT `tracktion::engine::`.

**Gotcha that binds every future wave-clip render test (hard-won here):** the headless
`renderEditToWav` tight pump `while (task.runJob()==jobNeedsRunningAgain){}` is fine for a
tone/synth (no file I/O) but **~20× too slow for a clip that reads audio off disk** — a
3.5 s wave render took **65 s** because the tight loop starves the engine's
`AudioFileManager`/cache async file reads of message-thread time. Fix: pump the message
loop a slice between blocks (`MessageManager::runDispatchLoopUntil(1)`, allowed because
`JUCE_MODAL_LOOPS_PERMITTED=1`). This mirrors the engine's own
`renderToAudioBufferDispatchingMessageThread`. After the fix the same render is **3.0 s**.
`RenderHelpers.h` now does this for all render tests; also added `regionRmsDbfs` +
`regionView` helpers (silence-before-clip and clip-body assertions).

**Render test** `tests/render/ImportClipRenderTest.cpp` `[render]`: render a 440 Hz tone to a
temp WAV, import it at t=2.0 s on a 4-track arrangement, render the whole edit, assert
(a) pre-clip region silent (−200 dBFS), (b) clip body 440 Hz at the source's −9.03 dBFS RMS,
(c) finite + body filled, (d) a missing-file import returns nullptr. Gate: `ctest --preset
dev` **11/11 green, 11.1 s** (import test 3.0 s); the Phase-0 tone render is unchanged at 2.4 s.

## 2026-06-15 — Phase 1 Chunk 2: ClipOps (move/trim/offset/gain/fades/delete)

GUI-free clip-edit operations in `src/engine/ClipOps.{h,cpp}` — the engine half of the
timeline's clip interactions, render-tested before any drag handle exists. Each op opens
its own undo transaction (`edit.getUndoManager().beginNewTransaction(name)`) so one
gesture == one undo step (the round-trip is proven by Chunk 3). Verified engine setters:

- `moveClip` → `Clip::setStart(pos, preserveSync=false, keepLength=true)` (source moves
  with the clip; same audio plays at the new position — confirmed by render).
- `setClipLength` → `Clip::setLength(dur, preserveSync=false)`; `setClipOffset` →
  `Clip::setOffset(dur)`; `deleteClip` → `Clip::removeFromParent()`.
- `setClipGainDb` → `AudioClipBase::setGainDB`; fades → `setFadeIn/Out` +
  `setFadeInType/OutType` (`AudioFadeCurve::Type` 1=linear…4=sCurve, mapped from a
  header-local `daw::FadeCurve` enum so ClipOps.h stays engine-free); `applyClipEdgeFades`.

**Upstream engine bug found (same class as the Spike 2 LoopInfo bug):**
`AudioClipBase::setFadeIn`/`setFadeOut` (`tracktion_AudioClipBase.cpp:511`) **always
return `false`** even on success — the no-overrun branch does `fadeIn = in; return false;`
and the function falls through to `return false`. The fade IS applied; only the bool is
wrong. So `daw::setClipFadeIn/Out` return **void**; verify via `getFadeIn()` and a render,
never the engine's bool. (Worth reporting on the Tracktion forum alongside the LoopInfo bug.)

**Render test** `tests/render/ClipOpsRenderTest.cpp` `[render]` (one source tone rendered
once, re-imported per section): move → tone appears at the new start, region before it
silent, body still 440 Hz at −9.03 dBFS; trim → tail where the untrimmed clip used to play
goes silent; gain −6 dB → body −15.03 dBFS; fade-in 0.4 s → clip start (−22.8 dBFS) well
below steady state (−9.0 dBFS); delete → track clip count → 0. Gate: `ctest --preset dev`
**12/12 green, 18.0 s** (ClipOps test ~6.8 s).

## 2026-06-15 — Phase 1 Chunk 3: undo/redo null test (perfect null)

`src/engine/EditUndo.{h,cpp}` — undo/redo facade over `edit.getUndoManager()`
(`undo`/`redo`/`canUndo`/`canRedo`/`clearUndoHistory`) for the UI's Undo/Redo commands,
plus `ensureUndoManagerReady`. GUI-free (forward-declared `te::Edit`).

**Engine gotcha (from the engine's own MidiList test):** an Edit attaches its
`UndoManager` via an **async message**, so a headless caller that mutates the Edit before
the message loop has run records **nothing** for undo. Fix: pump the loop once after
build/load (`MessageManager::runDispatchLoopUntil(20)`) — that's `ensureUndoManagerReady`.
A running GUI pumps continuously so it never needs it. With that pump + one
`beginNewTransaction` per gesture (Chunk 2), clip ops are recorded automatically (their
ValueTree mutations go through the Edit's UndoManager) — they take **no** explicit
`UndoManager*` argument, unlike MIDI note ops which do.

**Null test** `tests/render/UndoNullRenderTest.cpp` `[render]`: render baseline → move +
gain gestures (render differs by −6 dBFS) → undo,undo → re-render is a **perfect −200 dBFS
null** vs baseline → redo,redo → **perfect −200 dBFS null** vs the changed render. Added
`peakDiffDbfs` to RenderHelpers (max abs per-sample diff in dBFS; size mismatch = 0 dBFS so
it fails loudly). Gate: `ctest --preset dev` **13/13 green, 22.3 s**.

## 2026-06-15 — Phase 1 Chunk 4: ProjectSession (.tracktionedit lifecycle)

`src/engine/ProjectSession.{h,cpp}` — GUI-free new/open/save/saveAs over a
`.tracktionedit`, dirty-state, and a recents list. The canonical engine pattern (from
RecordingDemo/MidiRecordingDemo): `existsAsFile() ? loadEditFromFile : createEmptyEdit`,
then `edit->editFileRetriever = [file]{ return file; }`, save via
`EditFileOperations(*edit).save(true, true, false)`. Dirty = `hasChangedSinceSaved()`;
save resets it; `newProject` calls `markAsChanged()` (a new project is unsaved).

- **`flushState()` before save** — `save()` does not guarantee CachedValues (clip gain,
  fades) are pushed into the state tree, so ProjectSession::save calls `edit->flushState()`
  first. Without it a round-trip can drop a recently-set gain. (The Chunk-4 null test is
  the detector; with the flush the round-trip is a perfect null.)
- **Recents is GUI-free.** `juce::RecentlyOpenedFilesList` lives in **juce_gui_extra**,
  which daw_core must not link, so `src/engine/RecentFilesList.h` is a tiny header-only
  MRU list (newest-first, dedup, capped, newline persistence) the UI renders into a menu.
- `configureArrangementTracks` extracted from `buildArrangementEdit` so both the fileless
  builder (createSingleTrackEdit) and `newProject` (createEmptyEdit) share the
  "N tracks + neutral gains" shape.

**Render test** `tests/render/ProjectRoundTripRenderTest.cpp` `[render]`: newProject(2
tracks) + import clip + gain −6 dB → dirty true; render bufA; save → dirty false, file on
disk, in recents; **reopen in a fresh ProjectSession** → 2 tracks + 1 clip at the right
position; render bufB → **perfect −200 dBFS null** vs bufA. Plus: open of a missing file
returns false and leaves no edit. Gate: `ctest --preset dev` **14/14 green, 29.5 s**.

## 2026-06-15 — Phase 1 Chunk 5: ArrangementRenderer (export to WAV)

`src/engine/ArrangementRenderer.{h,cpp}` — `exportEditToWav(edit, dest, ExportOptions)`,
the GUI-free service behind the timeline's Export command. Same technique as
RenderHelpers (Renderer::RenderTask inline pump + message-loop slice between blocks for
wave clips) but a production-facing API: `ExportOptions{sampleRate, bitDepth, blockSize,
useMasterPlugins=true, failIfSilent, startSecs, endSecs<0=whole edit}` →
`ExportResult{success, errorMessage, lengthSeconds}`.

Kept **separate** from RenderHelpers (which stays a test fixture) so the passing render
tests are byte-for-byte unaffected. Note the engine `Renderer::Parameters` defaults:
`useMasterPlugins=false`, `usePlugins=true`, `canRenderInMono=true`,
`checkNodesForAudio=true` — so the exporter sets `useMasterPlugins` from options (defaults
**true**: a user's export should include the master fader, unlike the test fixture).

**Render test** `tests/render/ExportRenderTest.cpp` `[render]`: a two-clip arrangement
exported at 48k and 44.1k → both clips read back as 440 Hz, correct file sample rate,
length ≈ clip-B end, finite, errorMessage empty; and with the master cut −6 dB,
`useMasterPlugins=true` yields −15.03 dBFS vs `false` yields −9.03 dBFS. Gate:
`ctest --preset dev` **15/15 green, 32.5 s**.

## 2026-06-15 — Phase 1 Chunk 6: TimelineViewModel + TransportModel

GUI-free presentation model so the timeline UI stays dumb and the maths is unit-testable:

- **`TimelineViewModel`** (`src/engine/TimelineViewModel.h`, header-only, **engine-free**):
  pure pixel<->time mapping over a visible window + width — `timeToX`/`xToTime` (round-trip
  < 1 px), `zoomBy(factor, anchorX)` (keeps the time under the anchor fixed),
  `scrollByPixels`, `followPlayhead` (auto-follow when the playhead crosses the right
  margin). Testable with plain asserts, no engine/GUI.
- **Musical helpers** (`TimelineViewModel.cpp`, take `te::Edit&`): `snapSecondsToBar`/
  `snapSecondsToBeat` via `tempoSequence.toBeats`/`toTime` (round to the nearest beat
  multiple; bar = nearest multiple of the time-sig numerator), `barsBeatsText` (1-based
  "bar|beat" from `tempo::BarsAndBeats`), `minSecText`.
- **`TransportModel`** (`src/engine/TransportModel.{h,cpp}`): play/stop/togglePlay/loop/
  position over `Edit::getTransport()` (`ensureContextAllocated`+`play(false)`;
  `stop(false,false)` — clips follow the playhead, so no clearDevices teardown needed,
  unlike the Phase 0 continuous tone) + the bars:beats / min:sec readouts.

**Tests** `tests/render/TimelineMappingTest.cpp`: `[timeline]` pure-math case (mapping
round-trip, zoom-about-anchor, scroll, follow); `[render]` musical case at a pinned 120 bpm
4/4 (beat 0.5 s, bar 2 s) — bars:beats/min:sec/snap values exact, and a clip snapped
1.8 s→2.0 s renders silent up to the bar (−200 dBFS) then tone (−9.03 dBFS), tying the
snap maths to audio. Gate: `ctest --preset dev` **17/17 green, 37.0 s**.

## Pending (fill in when decided)

- ~~App name / bundle identifier~~: **EZStudio** / `com.parthivnair.ezstudio` (Chunk 2)
- ~~Tracktion Engine pinned SHA + JUCE submodule SHA~~: pinned (see 2026-06-11 Spike 2 entry)
- ~~Semantic-search path GO/NO-GO~~: **GO** (see 2026-06-11 Spike 1 entry)
- ~~Freesound OAuth2 redirect~~: registered as the paste-the-code page (see Spike 1 entry)
