# 06 — Landscape: Competitors, Reference DAWs, and Search-UX Inspiration

*Researched June 11, 2026. Web sources; verified facts vs. inference flagged where it matters.*

---

## 1. Cakewalk Next & Sonar (the UI reference point), mid-2026

**Verified:**
- Cakewalk by BandLab (the old free CbB) reached EOL **Dec 31, 2025** — fully sunsetted, activation disabled. Sonar is the successor; Next is the separate "modern/clip-based" DAW. ([Cakewalk Help](https://help.cakewalk.com/hc/en-us/articles/53866492346009), [PG Music forum thread](https://www.pgmusic.com/forums/ubbthreads.php?ubb=showflat&Number=856113))
- **Both Sonar and Next gained free tiers** (announced mid-2025); fully unlocked versions are gated behind **BandLab Membership** (~$14.95/mo, ~$149.50/yr renewal; promotional first-year pricing has appeared at $49–$79). Free tier is perpetual but requires periodic online sign-in. ([Sonar Free vs Membership](https://help.cakewalk.com/hc/en-us/articles/49546667908121), [Cakewalk forum announcement](https://discuss.cakewalk.com/topic/85542-cakewalk-sonar-and-next-now-available-with-free-tiers/), [BandLab pricing roundup](https://checkthat.ai/brands/bandlab/pricing))
- Next is **still developed, not discontinued/merged**: a March 20, 2026 product update says the team "turned inward to lay the groundwork for important updates anticipated throughout the year." Next's pitch: arrangement + lyrics tools, song templates; Membership unlocks BandLab Sounds (250k+ loops) and 6-stem separation. ([Cakewalk product update, Mar 20 2026](https://discuss.cakewalk.com/topic/94180-cakewalk-product-update-march-20-2026/), [Activate Next](https://help.cakewalk.com/hc/en-us/articles/34200598267289))

**Inference:** Next's release cadence is slow and it's clearly the second-priority product behind Sonar. The clean single-window/low-friction design is worth copying; the business model (free tier + cloud-account tether) is not a model a solo personal DAW needs — but it shows even BandLab-scale teams converge on "one window, fewer modes."

## 2. Waveform Free (Tracktion's own DAW)

**Verified:**
- Current as of 2026: **Waveform Free 13.5** — unlimited tracks, full VST3, no export limits, Win/macOS/Linux/RPi. Tracktion **rewrote the audio engine** (the same `tracktion_engine` you're using): much lower CPU, **perfect PDC in all routing configurations**, better many-core scaling. ([Tracktion product page](https://www.tracktion.com/products/waveform-free), [gearnews on 13.5](https://www.gearnews.com/tracktion-waveform-free-studio/), [MUSCO SOUND 2026 review](https://www.michaelmusco.com/2026/04/tracktion-waveform-free-review.html))

**What it proves for you:** the engine demonstrably supports a full commercial DAW — unlimited tracks, plugin hosting, modern graph-based PDC — so engine capability is *not* your risk. **UX notes:** Waveform is itself single-window; reviewers consistently praise capability but call its UI idiosyncratic (everything-is-a-track, bottom properties panel). *Inference:* treat Waveform as the engine-capability ceiling and an anti-pattern catalog for discoverability; treat Next as the look-and-feel target.

## 3. Open-source DAWs worth mining (status as of June 2026)

| DAW | Lang/Framework | License | Status (2026) |
|---|---|---|---|
| **Ardour** | C++ / GTK | GPL-2+ | Very active: 9.0 (Feb 5, 2026), **9.7 (Jun 5, 2026)** |
| **Zrythm** | C++23 / Qt-QML + JUCE | AGPL-3 | Active: **2.0 alpha (May 2026)** — full Qt/QML rewrite; v1.0 was Nov 2024 |
| **Stargate** | Python (UI) + C (engine) / PyQt | GPL-3 | **Dormant-ish**: last release 24.02.2, Jan 2025; no releases in ~17 months |
| **LMMS** | C++ / Qt6 | GPL-2+ | Active but slow: 1.3 milestone 84% done (Feb 2026), alpha imminent; Qt6 port complete |
| **openDAW** (andremichelle) | TypeScript / Web Audio, browser | AGPL-3 | Active; correct repo is **github.com/andremichelle/openDAW** (opendaw-studio repo deprecated); live at opendaw.studio |
| **Qtractor** | C++ / Qt | GPL-2+ | Active solo project: **1.6.0 (May 1, 2026)**, OSC support added |
| **Meadowlark** | Rust | GPL-3 | **Effectively dead**: GitHub org archived, indefinite hiatus, partial move to Codeberg, no resumed development |

Sources: [Ardour what's new](https://ardour.org/whatsnew.html), [linuxiac on 9.7](https://linuxiac.com/ardour-9-7-daw-released-with-better-midi-editing/), [Phoronix on Zrythm 2.0 alpha](https://www.phoronix.com/news/Zrythm-2.0-Alpha), [zrythm/zrythm](https://github.com/zrythm/zrythm), [stargate releases](https://github.com/stargatedaw/stargate/releases), [LMMS 1.3 milestone](https://github.com/LMMS/lmms/milestone/4), [andremichelle/openDAW](https://github.com/andremichelle/openDAW), [qtractor.org](https://qtractor.org/), [MeadowlarkDAW org](https://github.com/MeadowlarkDAW), [Billy Messenger's hiatus post](https://billydm.github.io/blog/why-im-taking-a-break-from-meadowlark/).

**Where each is useful to read** (a = waveform rendering, b = undo, c = timeline/editor interactions). *Mostly inference from codebase knowledge — verify in-repo before relying:*

- **Ardour** — the deepest reference for all three, but huge and GTK-idiomatic. (a) mature peak-file caching (`.peak` files, multi-zoom-level reads); (b) battle-tested `Command`/`MementoCommand` + serialized session history; (c) the canonical grab/drag "Drag" class hierarchy for timeline edits. Dense to read; best mined for *design*, not code lifting.
- **Zrythm v1/v2** — (b) explicit "UndoableAction" objects with serializable history; the cleanest mid-size undo design in the list. v2's QML timeline is young. **AGPL — read for patterns, do not copy code** into a GPLv3 project.
- **Qtractor** — small, single-author, very readable C++/Qt; (b) classic QUndoCommand-style command pattern; (c) compact timeline/clip editing code. Probably the best effort-to-insight ratio for a solo Qt-adjacent dev.
- **LMMS** — readable Qt C++, but (a) waveform code is basic and (c) editor code carries legacy; useful mainly as a "what 20 years of accretion looks like" caution.
- **Stargate** — Python UI makes interaction logic skimmable for (c) ideas, but dormant; engine in C is bespoke.
- **openDAW** — modern TypeScript, clean-room architecture; (a) canvas waveform rendering and (c) pointer-interaction state machines are concise and current. AGPL — patterns only.
- **Meadowlark** — code is frozen, but Billy Messenger's **["DAW Frontend Development Struggles"](https://billydm.github.io/blog/daw-frontend-development-struggles/)** post is essential solo-DAW reading (why GUI, not DSP, kills these projects).
- *(Note: since you're on Tracktion Engine, `tracktion_engine`'s own demos + Waveform behavior remain your primary (b) reference — the engine ships its own UndoManager-integrated edit model, so most undo problems are already solved upstream.)*

## 4. Splice & Loopcloud — search-UX inspiration only

**Splice (verified):**
- **Similar Sounds**: ML embedding search from any sample's "sonic signature," ignoring pack/tag metadata — results come from across the whole catalog. Shipped since ~2019. ([Splice blog: Introducing Similar Sounds](https://splice.com/blog/introducing-similar-sounds/))
- **CoSo / AI matching**: auto pitch-shifts and time-stretches the most-compatible sound so audition happens *in context* without manual search. ([Splice CoSo blog](https://splice.com/blog/coso-breakthrough-ai-tech/), [DJ Mag](https://djmag.com/tech/splices-new-app-uses-ai-match-millions-samples-seconds))
- **Sounds plugin (beta)**: lives in the DAW; "Listener" mode analyzes the playing track and surfaces harmonically matched sounds in real time; preview is key/BPM-synced to the project; every result drags straight onto a track. ([Sounds plugin](https://splice.com/tools/sounds-plugin), [plugin tips](https://splice.com/blog/splice-sounds-plugin-tips/))

**Loopcloud (verified):** filterable search (key/BPM/instrument/genre) + AI tagging/similarity via **Jamahook**; previews time-stretch/pitch-sync to project tempo+key *before* download; "find similar sounds" button on any result; drag from a "PROCESSED" button so the file lands already conformed to the project. ([Loopcloud features](https://www.loopcloud.com/cloud/features), [user manual PDF](https://www.loopmasters.com/lc/resources/LoopcloudUserGuide.pdf), [KVR listing](https://www.kvraudio.com/product/loopcloud-by-loopcloud))

**Distilled UX pattern for your browser:** (1) preview always in project key/tempo, during playback; (2) waveform thumbnails directly in result rows; (3) one-click "more like this" from any result; (4) drag delivers the *already-conformed* audio. All four are implementable locally over your own sample folder (key/BPM analysis + an audio-embedding model) with zero API dependence.

## Hurdle framing (brief)

- **Why solo DAWs die:** Meadowlark is the type specimen — frontend/GUI scope, not DSP, is the killer; plus the social weight of running a public project. Qtractor survives by staying *small, boring, and Qt-native* for 20 years. **Implication:** you've dodged the biggest hurdle by adopting Tracktion Engine (engine + undo + PDC solved); the remaining risk is UI scope creep.
- **Scope discipline:** even BandLab slowed Next's cadence to "harden core strengths." Ship the Next-like single window over a proven engine; mine Ardour/Qtractor for interaction patterns instead of inventing them; treat sample-search UX (Section 4) as a bounded, high-payoff differentiator rather than a platform.
- **License hygiene:** Ardour/LMMS/Qtractor (GPL-2+) and Stargate/Meadowlark (GPL-3) are code-compatible with your GPLv3 project; Zrythm and openDAW are **AGPL** — patterns yes, code no.
