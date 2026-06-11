# 05 — Claude Code Development Workflow for a Personal DAW (Tracktion Engine + JUCE 8, C++20, macOS)

**Researched:** 2026-06-11. Primary sources: code.claude.com docs (fetched live), GitHub repos (pamplejuce, tracktion_engine, rtsan, farbot, crill), LLVM docs, JUCE forum. Facts are cited; items marked **[inference]** are my synthesis or recalled detail not re-verified today.

---

## Key hurdles (read this first)

1. **C++ build latency vs. AI iteration speed.** Claude Code's agentic loop (edit → build → test → read output → fix) is only as fast as your build. A cold JUCE+Tracktion build is many minutes; if incremental builds take 2–5 min, every AI iteration costs that. Target: **sub-30 s incremental link+build** of the target you're testing via Ninja + compiler cache + PCH + small TUs, and a console *test* target that doesn't link the full GUI app.
2. **Audio is unverifiable by reading code or looking at a screen.** Claude cannot listen. Without machine-checkable audio assertions, you become the verification loop for every DSP change. The fix is the core of this doc: **offline render-to-WAV tests** (Tracktion `Renderer`) + golden-file/null/metric comparisons that return pass/fail exit codes Claude can iterate against. Anthropic's own best-practices doc makes this the #1 lever: *"Give Claude a check it can run… the loop closes on its own."* ([code.claude.com/docs/en/best-practices](https://code.claude.com/docs/en/best-practices), fetched 2026-06-11)
3. **Real-time bugs are invisible to normal tests.** An allocation or mutex on the audio thread passes every unit test and renders bit-identical WAVs, then glitches live. RealtimeSanitizer (RTSan) is the right tool — **and it supports macOS natively** ([rtsan README](https://github.com/realtime-sanitizer/rtsan)). You run it *in your normal build/test lane* — no separate sanitizer box — plus code conventions an AI can mechanically follow.
4. **macOS CI is fast but billed at a premium.** GitHub-hosted macOS runners are now Apple-silicon (M-class) and among the fastest hosted runners, but they are **billed at a 10× per-minute multiplier** vs Linux (and the free-tier minutes are consumed 10× as fast). Mitigate with ccache + JUCE/Tracktion source caching, and lean on a cheap Linux lane for the high-volume test loop if minutes become a concern; expect a few minutes warm, ~15–30 min cold for an app this size on M-class runners **[inference — verify current runner specs/pricing]**.
5. **Tracktion Engine is a huge dependency you don't control.** Engine + JUCE dominate compile time and context. Keep them out of Claude's context (never let it read module internals unprompted; point it at headers/docs), and pin exact commits so golden renders stay stable.

---

## 1. Claude Code setup for a large C++/CMake codebase (state of mid-2026)

Primary source: **"Best practices for Claude Code"**, [code.claude.com/docs/en/best-practices](https://code.claude.com/docs/en/best-practices) (current docs, fetched 2026-06-11). This supersedes/extends the original Anthropic engineering post *"Claude Code: Best practices for agentic coding"* (anthropic.com/engineering/claude-code-best-practices, Apr 2025 — now 403/moved; content folded into the docs). Also: ["How Claude Code works in large codebases"](https://claude.com/blog/how-claude-code-works-in-large-codebases-best-practices-and-where-to-start) (claude.com blog, 2026) — key points: no central index, Claude navigates like an engineer (grep/read/follow references), so **repo legibility is the multiplier**; hierarchical CLAUDE.md + deterministic hooks + LSP matter more than raw model capability ("the harness does sixty percent").

### CLAUDE.md conventions (verified guidance)
- Keep it **short**; bloated CLAUDE.md files cause rules to be ignored. Test: "Would removing this line cause mistakes?"
- Include: exact build/test/run commands, project-specific gotchas, repo etiquette, architectural decisions. Exclude: anything Claude can infer by reading code.
- Hierarchical placement: root `CLAUDE.md` + per-directory `CLAUDE.md` (pulled in on demand when Claude reads files there) — ideal for `src/engine/`, `src/ui/`, `tests/` having different rules.
- `@path/to/file` imports; `CLAUDE.local.md` for personal notes (gitignored).
- The acceptance test from Anthropic's large-codebase guidance: a fresh session told "run the tests" must succeed first try.

Recommended root CLAUDE.md skeleton for this project:

```markdown
# DAW project (Tracktion Engine + JUCE 8, C++20, macOS)

## Build & test (use these EXACTLY)
- Configure: cmake --preset dev
- Build app: cmake --build --preset dev --target DAWApp
- Build+run tests: cmake --build --preset dev --target daw_tests && ctest --preset dev
- Render-test one case: build/dev/tests/daw_tests "[render]" (Catch2 tag filter)
- NEVER use the Xcode generator or open .xcodeproj; we build with Ninja + clang only.

## Audio-thread rules (HARD RULES — see docs/realtime-rules.md)
- No allocation, locks, file/network IO, or logging in processBlock /
  anything reachable from the audio callback.
- Cross-thread communication: use the FIFO/RealtimeObject wrappers in
  src/engine/rt/. Never add a std::mutex touching audio code.
- Any function intended for the audio thread is annotated RT_NONBLOCKING
  (expands to [[clang::nonblocking]] on clang builds).

## Verification
- DSP changes MUST come with or update a render test in tests/render/.
- Run tests and paste output before claiming done.

## Gotchas
- tracktion_engine and JUCE are pinned submodules — never edit them.
- Edits (.tracktionedit) used as test fixtures live in tests/fixtures/.
```

### Repo structure for AI navigability **[inference, grounded in the large-codebase guidance]**
- Small files, one class/concern per file; Claude greps and reads excerpts, so 200–500-line files beat 3,000-line ones.
- Clear module boundaries: `src/engine/` (Tracktion glue, no JUCE GUI includes), `src/ui/`, `src/rt/` (real-time utilities), `tests/`. Header hygiene (IWYU, forward declarations) cuts both compile time and the context Claude must read.
- Keep third-party code in `libs/` or FetchContent so greps don't drown in JUCE internals (`.claudeignore`-style exclusion isn't needed — Claude greps scoped paths if CLAUDE.md says so).

### Tooling: LSP plugin, MCP, skills, hooks (verified)
- **clangd LSP plugin is official in 2026**: Claude Code added LSP support in v2.0.74; an official [Clangd LSP plugin](https://claude.com/plugins/clangd-lsp) exists, requiring `compile_commands.json` (CMake: `CMAKE_EXPORT_COMPILE_COMMANDS=ON`). Gives symbol navigation + diagnostics after edits. Known rough edges: ["AST for non-added document" bug #29501](https://github.com/anthropics/claude-code/issues/29501), [C++20 modules config issue #23826](https://github.com/anthropics/claude-code/issues/23826) — fine since JUCE doesn't use C++20 modules. Community alternatives: [boostvolt/claude-code-lsps](https://github.com/boostvolt/claude-code-lsps), [zircote/cpp-lsp](https://github.com/zircote/cpp-lsp) (clangd + clang-tidy + clang-format bundled).
- **Hooks** (deterministic, unlike CLAUDE.md): PostToolUse hook running `clang-format -i` on edited `.h/.cpp`; a **Stop hook** that runs `cmake --build --preset dev --target daw_tests && ctest` and blocks the turn ending until green (docs: Stop hook gates "as a deterministic gate"; Claude overrides after 8 consecutive blocks). Avoid build-on-every-edit hooks if builds exceed ~30 s — it stalls the loop; build at turn end instead **[inference]**.
- **Skills** (`.claude/skills/*/SKILL.md`): good fits here — `render-test` (how to add a golden render test), `tracktion-api` (Edit/Renderer/TransportControl cheat-sheet with links), `rt-review` (the audio-thread checklist). Skills load on demand and keep CLAUDE.md slim (per docs).
- **Subagents**: use for codebase investigation ("use a subagent to find how Tracktion exposes plugin automation") so JUCE/Tracktion file dumps don't pollute main context; and as **adversarial reviewer** of diffs against the RT rules (docs recommend fresh-context review; `/code-review` skill ships built-in).
- **Plan mode**: explore → plan → implement → commit, for multi-file changes; skip for one-liners (docs).
- **MCP**: less critical than the LSP for this project; useful ones: GitHub MCP for CI logs, and a docs-lookup MCP only if you find yourself repeatedly fetching JUCE/Tracktion docs **[inference]**.
- **Headless/CI**: `claude -p "..."` for scripted fixes; `--allowedTools` to scope.

---

## 2. Fast iteration: CMake + Ninja + caching for JUCE/Tracktion

### What the ecosystem does (verified)
- **[pamplejuce](https://github.com/sudara/pamplejuce)** (sudara; JUCE 8, Catch2 ~3.7.x, pluginval, GitHub Actions) is the reference template. Its CI includes a **macos-14** lane, pluginval v1.0.3 at `--strictness-level 10`, `ctest` post-build (verified from [build_and_test.yml](https://github.com/sudara/pamplejuce/blob/main/.github/workflows/build_and_test.yml), fetched 2026-06-11). On macOS it builds with **AppleClang** (the default toolchain) and caches with ccache. Reusable CMake bits live in [sudara/cmake-includes](https://github.com/sudara/cmake-includes).
- **ccache** is the standard, battle-tested compiler cache for clang on macOS ([ccache](https://ccache.dev/), [hendrikmuhs/ccache-action](https://github.com/hendrikmuhs/ccache-action) supports macOS). Hook in via `CMAKE_{C,CXX}_COMPILER_LAUNCHER=ccache`. ccache handles clang `-g` debug info cleanly, so there's no debug-format caveat to work around. Set a generous `CCACHE_MAXSIZE` since JUCE/Tracktion objects are large.
- **juceaide gotcha**: JUCE's build-time helper tool is configured in a sub-build that historically did **not** forward `CMAKE_*_COMPILER_LAUNCHER`, so it recompiles uncached on clean configures ([JUCE forum FR](https://forum.juce.com/t/fr-improve-the-performance-of-building-juceaide-by-forwarding-compiler-launcher-cmake-args/61543)). Workaround: cache the whole build dir in CI, or pass `-DJUCE_BUILD_HELPER_TOOLS` tricks **[inference]**.

### PCH and unity builds with JUCE
- JUCE modules are already amalgamated ("unity-like": each module = one big TU), so unity builds help **your** code, not JUCE's. Measured ecosystem numbers: PCH alone 20–40 % faster; unity+PCH ~25 % over either ([Alexander Houghton, "Faster Compiling: VS Unity Builds"](https://alexanderhoughton.co.uk/blog/faster-compiling-visual-studio-unity-jumbo-builds/); [JUCE forum: "Modules Compilation Speed"](https://forum.juce.com/t/juce-modules-compilation-speed-whats-the-trick/50811)).
- **Recommendation**: `target_precompile_headers` with `<JuceHeader.h>`-equivalent (juce + tracktion umbrella headers) on your app target; **skip unity builds** — ODR collisions are common (anonymous-namespace helpers, `using namespace juce`) and unity builds *worsen* incremental rebuild time of a single edited file, which is exactly the AI-loop metric you care about **[inference + forum-reported ODR pain]**.
- Other wins **[inference, standard practice]**: Ninja (not the Xcode generator), the fast **Apple linker** (the `ld-prime`/parallel linker default in Xcode 15+) or `lld` where it helps, split a thin `daw_core` static lib (engine logic, no GUI) that the test runner links — so DSP test iterations never relink the full app; keep heavy Tracktion includes out of your own headers. (`mold` is ELF-only and does not target Mach-O, so it's not an option on macOS.)

### CMakePresets.json pattern (recommended)

```jsonc
{
  "version": 6,
  "configurePresets": [
    {
      "name": "dev",
      "generator": "Ninja",
      "binaryDir": "build/dev",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_COMPILER_LAUNCHER": "ccache",
        "CMAKE_C_COMPILER_LAUNCHER": "ccache",
        "CMAKE_OSX_ARCHITECTURES": "arm64",                 // native Apple-silicon dev build
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",              // clangd / Claude LSP
        "DAW_BUILD_TESTS": "ON"
      }
    },
    { "name": "release", "inherits": "dev", "binaryDir": "build/release",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release", "DAW_BUILD_TESTS": "OFF",
        "CMAKE_OSX_ARCHITECTURES": "arm64;x86_64" } },     // universal2 for distribution
    { "name": "rtsan", "inherits": "dev", "binaryDir": "build/rtsan",
      "cacheVariables": {
        // use Homebrew LLVM clang >= 20 — Apple's bundled clang may lack the realtime runtime
        "CMAKE_C_COMPILER": "/opt/homebrew/opt/llvm/bin/clang",
        "CMAKE_CXX_COMPILER": "/opt/homebrew/opt/llvm/bin/clang++",
        "CMAKE_CXX_FLAGS": "-fsanitize=realtime", "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=realtime" } },
    { "name": "asan", "inherits": "dev", "binaryDir": "build/asan",
      "cacheVariables": { "CMAKE_CXX_FLAGS": "-fsanitize=address,undefined",
        "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=address,undefined" } }
  ],
  "buildPresets": [{ "name": "dev", "configurePreset": "dev" }],
  "testPresets": [{ "name": "dev", "configurePreset": "dev", "output": { "outputOnFailure": true } }]
}
```

Presets are ideal for Claude: one canonical command per intent, stated in CLAUDE.md — no flag-guessing.

---

## 3. Automated verification without human listening (the critical piece)

### 3.1 Offline render tests with Tracktion Engine (verified API)
Tracktion Engine ships exactly the needed primitive: `tracktion::engine::Renderer` with static `renderToFile(...)` rendering an `Edit` over a time range, with options for plugin inclusion, normalization, channel layout ([tracktion_Renderer.h](https://github.com/Tracktion/tracktion_engine/blob/master/modules/tracktion_engine/model/export/tracktion_Renderer.h); forum threads confirm widespread offline-render use, e.g. ["How to render an Edit to a new audio file"](https://forum.juce.com/t/how-to-render-an-edit-to-a-new-file-audio-file/35136)). Tracktion's own repo builds **TestRunner** and **Benchmark** CMake targets, with engine unit tests behind `TRACKTION_UNIT_TESTS=1` (juce::UnitTest-based) ([tracktion_engine README](https://github.com/Tracktion/tracktion_engine)) — read those tests as worked examples of constructing Edits in code.

**Pattern**: each DSP/engine feature gets a Catch2 test that (a) builds an Edit programmatically or loads a `.tracktionedit` fixture, (b) `Renderer::renderToFile` to a temp WAV (headless `Engine` — no audio device needed), (c) asserts against the strategies below. Caveat from forum reports: ensure plugins are fully initialized before render and prefer deterministic sources (no live input, no time-stretch nondeterminism) **[inference from forum threads]**.

### 3.2 Comparison strategies (tiered)
1. **Sample-exact golden files** — `memcmp` of float samples vs a committed reference WAV. Strongest signal, but brittle: breaks on any intentional change, on engine version bumps, sometimes across CPUs (FMA/denormal differences). Use for: pure routing/gain/mix correctness with pinned dependency commits. Store goldens in Git LFS; regen via a dedicated tool target (`daw_regen_goldens`) so Claude can propose-but-not-silently-overwrite **[inference]**.
2. **Tolerance-based comparison** — per-sample peak error and RMS error under thresholds (e.g. peak < −96 dBFS, RMS < −120 dBFS difference). The default workhorse. [sudara/melatonin_test_helpers](https://github.com/sudara/melatonin_test_helpers) provides Catch2 matchers for JUCE AudioBlocks (`isEqualTo`, channel-wise checks, etc.), and melatonin_audio_sparklines prints waveform "sparklines" into failure messages — excellent for an AI reading test output.
3. **Null tests** — render A (reference path) and B (refactored path), subtract, assert residual ≈ silence. Perfect for refactors: "new graph code must null against old". Audio-engineering staple ([Gearspace null-test discussion](https://gearspace.com/board/so-much-gear-so-little-time/463539-null-test-plugins.html)); requires exact time alignment.
4. **Feature/metric assertions** — when output is intentionally different or nondeterministic: RMS/peak levels per region, LUFS via `juce::LoudnessMeter`-style or libebur128, THD of a rendered sine through a saturator, FFT-bin energy checks ("filter at 1 kHz attenuates 4 kHz by ≥ 24 dB"), silence/non-silence, click detection (max inter-sample delta). Spectral comparison (band-wise dB tolerance) is the most change-tolerant tier **[inference; spectral-matching is standard, see e.g. TDR Prism-style analysis]**.
5. **Plugin-hosting smoke tests** — [pluginval](https://github.com/Tracktion/pluginval) (Tracktion's own validator) against every third-party plugin you host and against any plugin you build; catches lifecycle bugs (e.g. parameter listeners used after destruction — [melatonin blog on pluginval](https://melatonin.dev/blog/pluginval-is-a-plugin-devs-best-friend/)). pamplejuce runs it at `--strictness-level 10` in CI.

**Test pyramid for this DAW [inference]:** many fast pure-DSP unit tests (process buffers directly, no engine) → dozens of render tests (1–5 s Edits, tolerance/null) → a few end-to-end golden renders of a "kitchen sink" project → pluginval lane. Keep render fixtures short so the whole suite stays < 1 min — that's what lets a Stop hook gate every Claude turn.

### 3.3 UI verification
- `juce::Component::createComponentSnapshot()` renders any component to a `juce::Image` offscreen — screenshot-based regression is feasible: snapshot → PNG → perceptual/pixel diff vs golden, with known limitation that modal popups (ComboBox menus) aren't captured ([JUCE forum](https://forum.juce.com/t/createcomponentsnapshot/507)). No established JUCE visual-regression framework exists — roll a small Catch2 harness; allow per-pixel tolerance for font-rendering differences across machines, or run UI snapshots only on one pinned CI OS **[inference]**.
- [melatonin_inspector](https://github.com/sudara/melatonin_inspector) for interactive layout debugging (Figma-style component inspector, FPS meter, paint timing) — and screenshots of the running app can be pasted straight into Claude for "match this design" loops (docs recommend the screenshot-compare workflow).

---

## 4. Real-time-safety tooling

### RTSan — verified status (June 2026)
- **In upstream LLVM since v20.0.0** (released March 2025); enable with `-fsanitize=realtime` and mark audio entry points `[[clang::nonblocking]]`. Detects malloc/free/pthread_mutex_lock/syscalls at runtime *within* nonblocking contexts; `[[clang::blocking]]` marks your own unsafe functions; `__rtsan::ScopedDisabler` and suppression files for false positives; `RTSAN_OPTIONS` env config ([LLVM RealtimeSanitizer docs, release/20.x](https://github.com/llvm/llvm-project/blob/release/20.x/clang/docs/RealtimeSanitizer.rst); [rtsan hub](https://github.com/realtime-sanitizer/rtsan)).
- **macOS is a first-class supported platform** ([rtsan README](https://github.com/realtime-sanitizer/rtsan/blob/main/README.md): supported on Linux, macOS, iOS, recently FreeBSD). Ecosystem is active (Rust RFC [#3766](https://github.com/rust-lang/rfcs/pull/3766), standalone wrappers updated in 2026; real projects adopting it, e.g. [LMMS PR #7764](https://github.com/LMMS/lmms/pull/7764)).
- **Practical consequence:** run RTSan **natively in your normal macOS build/test lane**. The one caveat: **use Homebrew LLVM clang ≥ 20** (`brew install llvm`), because Apple's bundled Xcode clang may not ship the `-fsanitize=realtime` runtime. Your render tests double as the RTSan workload (offline render exercises the same processing graph; pair with a real playback smoke run through a CoreAudio or AudioWorkgroup test device) **[inference]**.
- Annotate now anyway: define `RT_NONBLOCKING` → `[[clang::nonblocking]]` (you're on clang everywhere on macOS, so it's always live). The attribute also enables clang's compile-time *function-effect analysis* warnings (`-Wfunction-effects`) on every build.

### ThreadSanitizer
TSan finds data races (a different bug class from RT-safety) and, like ASan, **runs natively with clang on macOS** — run both over your multithreaded engine tests in the macOS lane (`-fsanitize=thread` and `-fsanitize=address,undefined`; don't combine TSan with ASan in one build). No separate platform is needed for any of the sanitizers.

### Lock-free libraries (verified existence/scope)
- **[farbot](https://github.com/hogliux/farbot)** — Fabian Renn-Giles' "Realtime Box o' Tricks" (Meeting C++ 2019): `RealtimeObject` (safe RT/non-RT shared state with scoped access), flexible `FIFO` (SPSC/MPMC, configurable failure modes), `AsyncCaller` (defer lambdas off the audio thread). MIT. Low commit count, no releases — treat as reference implementation to study/vendor rather than a maintained dependency **[status verified; usage advice inference]**.
- **[crill](https://github.com/crill-dev/crill)** — header-only, cross-platform real-time utilities (Timur Doumler et al. **[inference — authorship recalled]**); includes progressive-backoff spin mutex and seqlock-style objects per its docs/talks **[inference — component list recalled, repo fetch didn't enumerate]**. WIP, no releases, active CI.
- **[moodycamel::ReaderWriterQueue](https://github.com/cameron314/readerwriterqueue)** — fast SPSC lock-free queue, drop-in headers; the pragmatic default for UI↔audio messaging. [ConcurrentQueue](https://github.com/cameron314/concurrentqueue) (MPMC) scales with threads but has weaker SPSC throughput. Note: `enqueue` may allocate; use `try_enqueue` on the RT side or pre-size **[inference — documented behavior, verify]**.

### Conventions an AI can enforce (recommended)
- Single choke point: all cross-thread traffic through `src/rt/` wrappers; CLAUDE.md rule "never add mutex/allocation in files under src/engine/dsp/".
- Debug assert layer: a `RT_CHECK` guard that, in debug builds, intercepts allocations on the audio thread (replace global new with thread-flag check, JUCE's `juce::ScopedNoDenormals`-style RAII marker) — the classic "AudioGuard" pattern **[inference; no single canonical library — pamplejuce ecosystem has no equivalent, roll ~50 lines]**.
- Reviewer subagent prompt: "scan this diff for allocations, locks, IO, logging, shared_ptr copies, or unbounded loops reachable from processBlock; report file:line" — mechanical enough for high recall.
- grep-able denylist in CI: `rg "std::mutex|malloc|new\s" src/engine/dsp/` as a cheap pre-RTSan tripwire **[inference]**.

---

## 5. CI: GitHub Actions skeleton

Verified ingredients: pamplejuce's [build_and_test.yml](https://github.com/sudara/pamplejuce/blob/main/.github/workflows/build_and_test.yml) (multi-OS matrix incl. macos-14, pluginval 1.0.3, ctest, per-OS artifacts, `cancel-in-progress`), [hendrikmuhs/ccache-action](https://github.com/hendrikmuhs/ccache-action) (works on macOS + Linux), macOS-runner cost ([BuildJet analysis](https://buildjet.com/for-github-actions/blog/a-performance-review-of-github-actions-the-cost-of-slow-hardware) — note macOS runners bill at a 10× minute multiplier).

```yaml
name: build-and-test
on: [push, pull_request]
concurrency: { group: "${{ github.ref }}", cancel-in-progress: true }

jobs:
  build:
    strategy:
      fail-fast: false
      matrix:
        include:
          - { name: macos, os: macos-14,    preset: dev }  # Apple-silicon, primary target
          - { name: linux, os: ubuntu-24.04, preset: dev }  # optional cheap cross-check (10× fewer billed minutes)
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
        with: { submodules: recursive }   # JUCE + tracktion_engine pinned; submodule checkout ~ FetchContent but cache-friendlier
      - uses: hendrikmuhs/ccache-action@v1   # works on macOS + Linux
      - name: Linux deps
        if: matrix.name == 'linux'
        run: sudo apt-get update && sudo apt-get install -y ninja-build libasound2-dev libcurl4-openssl-dev libx11-dev libxinerama-dev libxext-dev libfreetype6-dev libwebkit2gtk-4.1-dev libglu1-mesa-dev
      - name: macOS deps
        if: matrix.name == 'macos'
        run: brew install ninja ccache
      - run: cmake --preset ${{ matrix.preset }}
      - run: cmake --build --preset ${{ matrix.preset }} --parallel
      - run: ctest --preset ${{ matrix.preset }}          # unit + render-to-WAV golden tests, headless
      - name: Upload failed-render diffs
        if: failure()
        uses: actions/upload-artifact@v4
        with: { name: "render-diffs-${{ matrix.name }}", path: build/**/render-out/*.wav }

  rtsan:   # RT-safety lane — runs natively on macOS (RTSan supports macOS)
    runs-on: macos-14
    steps:
      - uses: actions/checkout@v4
        with: { submodules: recursive }
      - run: brew install llvm ninja   # Homebrew LLVM clang >= 20 ships the realtime runtime
      - run: cmake --preset rtsan && cmake --build --preset rtsan --target daw_tests
      - run: ctest --preset rtsan   # render tests under -fsanitize=realtime
```

Notes:
- **Realistic times [inference]:** macOS M-class runners are fast — a few minutes warm (ccache), ~15–30 min cold for app+engine+tests (link dominates). They bill at a **10× minute multiplier**, so if you're burning free minutes, push the high-frequency test/sanitizer loop onto the cheap Linux lane and treat the macOS lane as the primary build/correctness + RTSan gate. Consider BuildJet-class third-party runners (cheaper per minute) if cost becomes the bottleneck.
- Cache JUCE/Tracktion as **pinned submodules** (deterministic goldens; FetchContent works too but re-fetch + juceaide rebuild cost more without careful caching — see juceaide launcher issue above).
- Optionally add a pluginval job if/when you ship a plugin target; for a DAW, instead host pluginval's *test plugins* in render tests **[inference]**.
- Upload failing render WAVs + diff WAVs as artifacts — Claude can't listen, but you can, and Claude can read the metric printout.

---

## 6. Concrete recommended setup (summary)

1. **Presets**: `dev` (Ninja + AppleClang, ccache, compile_commands), `release` (universal2), `rtsan` (Homebrew LLVM clang ≥ 20, native macOS), `asan`. PCH on app + core lib; no unity builds; `daw_core` static lib + console `daw_tests` so DSP iteration never links the GUI.
2. **Claude harness**: lean root CLAUDE.md (commands + RT rules + verification policy), per-dir CLAUDE.md for `src/engine` and `tests`, official clangd LSP plugin, clang-format PostToolUse hook, Stop hook = build+ctest gate, skills for render-test authoring and Tracktion API notes, subagents for exploration and RT-rule diff review, plan mode for multi-file work.
3. **Test strategy**: Catch2 + melatonin_test_helpers; pyramid = buffer-level DSP tests → 1–5 s `Renderer::renderToFile` tests (tolerance default, null tests for refactors, sample-exact goldens only with pinned deps) → metric/spectral assertions for "musical" behavior → pluginval for hosted-plugin smoke → optional `createComponentSnapshot` PNG regression on one pinned OS. Goldens in Git LFS; explicit regen target.
4. **RT safety**: `RT_NONBLOCKING` macro now; RTSan, TSan and ASan all run **natively on macOS** (RTSan via Homebrew LLVM clang ≥ 20); moodycamel SPSC (or farbot/crill patterns) behind a single `src/rt/` facade; grep tripwires + reviewer-subagent checklist as a cheap complement that runs on every diff.
5. **CI**: macOS (AppleClang + ccache-action) primary lane with native RTSan/sanitizer jobs, optional cheap Linux cross-check, submodule-pinned deps, ctest render suite headless, failure artifacts = WAV diffs.

---

## Sources

- Claude Code best practices (official docs, current): https://code.claude.com/docs/en/best-practices (fetched 2026-06-11)
- How Claude Code works in large codebases (claude.com blog, 2026): https://claude.com/blog/how-claude-code-works-in-large-codebases-best-practices-and-where-to-start
- Original Anthropic engineering post (Apr 2025, now folded into docs): https://www.anthropic.com/engineering/claude-code-best-practices
- Clangd LSP plugin: https://claude.com/plugins/clangd-lsp ; issues: https://github.com/anthropics/claude-code/issues/29501 , https://github.com/anthropics/claude-code/issues/23826
- pamplejuce: https://github.com/sudara/pamplejuce ; workflow: https://github.com/sudara/pamplejuce/blob/main/.github/workflows/build_and_test.yml ; cmake-includes: https://github.com/sudara/cmake-includes
- Melatonin blog — CMake with JUCE: https://melatonin.dev/blog/how-to-use-cmake-with-juce/ ; pluginval: https://melatonin.dev/blog/pluginval-is-a-plugin-devs-best-friend/ ; tips list: https://melatonin.dev/blog/big-list-of-juce-tips-and-tricks/
- melatonin_test_helpers (Catch2 AudioBlock matchers): https://github.com/sudara/melatonin_test_helpers ; melatonin_inspector: https://github.com/sudara/melatonin_inspector
- Tracktion Engine: https://github.com/Tracktion/tracktion_engine ; Renderer: https://github.com/Tracktion/tracktion_engine/blob/master/modules/tracktion_engine/model/export/tracktion_Renderer.h ; render forum threads: https://forum.juce.com/t/how-to-render-an-edit-to-a-new-file-audio-file/35136 , https://forum.juce.com/t/how-to-properly-use-renderer-rendertofile-is-waveform-s-render-to-file-the-same-thing/66792
- pluginval: https://github.com/Tracktion/pluginval
- RTSan: https://github.com/realtime-sanitizer/rtsan (supports macOS, README) ; LLVM docs (20.x): https://github.com/llvm/llvm-project/blob/release/20.x/clang/docs/RealtimeSanitizer.rst ; Clang 20.1 release docs: https://releases.llvm.org/20.1.0/tools/clang/docs/RealtimeSanitizer.html ; LMMS adoption: https://github.com/LMMS/lmms/pull/7764 ; Rust RFC: https://github.com/rust-lang/rfcs/pull/3766
- farbot: https://github.com/hogliux/farbot ; crill: https://github.com/crill-dev/crill ; moodycamel: https://github.com/cameron314/readerwriterqueue , https://github.com/cameron314/concurrentqueue
- Build speed: ccache: https://ccache.dev/ ; ccache-action (macOS + Linux): https://github.com/hendrikmuhs/ccache-action ; juceaide launcher FR: https://forum.juce.com/t/fr-improve-the-performance-of-building-juceaide-by-forwarding-compiler-launcher-cmake-args/61543 ; unity builds: https://alexanderhoughton.co.uk/blog/faster-compiling-visual-studio-unity-jumbo-builds/ ; JUCE compile-speed thread: https://forum.juce.com/t/juce-modules-compilation-speed-whats-the-trick/50811
- CI runner performance / cost (macOS billed 10×): https://buildjet.com/for-github-actions/blog/a-performance-review-of-github-actions-the-cost-of-slow-hardware ; GitHub-hosted runner pricing/multipliers: https://docs.github.com/en/billing/managing-billing-for-github-actions/about-billing-for-github-actions ; CI for audio plugins: https://moonbase.sh/articles/continuous-integration-for-audio-plugins-tips-tricks-gotchas/ (403 on fetch; cited from search index)
- JUCE current version / macOS (Xcode + macOS SDK support): https://github.com/juce-framework/JUCE/releases
- JUCE macOS renderer (CoreGraphics / optional Metal layer): https://github.com/juce-framework/JUCE/blob/master/BREAKING_CHANGES.md
- UI snapshots: https://forum.juce.com/t/createcomponentsnapshot/507 ; null tests: https://gearspace.com/board/so-much-gear-so-little-time/463539-null-test-plugins.html
