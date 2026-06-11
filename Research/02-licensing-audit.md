# 02 — Licensing Audit

**Project context:** Personal, non-commercial DAW for macOS. Tracktion Engine + JUCE, C++20. The whole codebase can be released under GPLv3 (or stronger copyleft). No revenue, no closed-source distribution planned.

**Audit date:** 2026-06-11.
**Method:** Primary sources (license texts in upstream repos, official docs) verified via direct fetch where possible. Several official web pages (juce.com, steinberg.net, freesound.org, breakfastquay.com) blocked automated fetching (HTTP 403); for those, terms were verified via raw files in the official GitHub repos and corroborating secondary sources. Anything not verified against a primary source is explicitly flagged below.

---

## Headline findings (read this first)

1. **Audio I/O carries no licensing surface: CoreAudio is a system framework.** The native low-latency audio API on macOS is **CoreAudio**, which JUCE drives through `juce_audio_devices` — **no SDK to vendor, no license to accept, no redistribution question**. The one SDK-licensing item that matters for hosting is VST3: **the VST3 SDK is now MIT** (as of VST 3.8, Oct 2025) — verified at primary-source level: the `steinbergmedia/vst3sdk` repo `LICENSE.txt` is the MIT text, "Copyright (c) 2025, Steinberg Media Technologies GmbH", and the JUCE 8.0.11 changelog says *"Updated the VST3 SDK to 3.8.0 (MIT license)"*.
2. **JUCE 8's open-source option is AGPLv3, not GPLv3.** This is compatible with a GPLv3 project (GPLv3 §13), but the combined binary is effectively AGPL-encumbered. Decide up front: license your own code "GPLv3 or later" (recommended) and note that the combined work includes AGPLv3 components.
3. **Freesound is the main remaining obligations surface:** API is free for **non-commercial use only**, original-quality downloads require **OAuth2**, attribution to Freesound + uploaders is mandatory, and you must not replicate/mirror Freesound's database. CC-BY-NC sounds impose downstream restrictions on anything users render from their projects.
4. Everything else in the stack is MIT / CC0 / LGPL / GPL-compatible. **No component blocks a GPLv3 release.**

---

## Summary table

| Item | License | Cost for this project | Obligations | Gotchas |
|---|---|---|---|---|
| JUCE 8 | Dual: **AGPLv3** or commercial EULA (Starter free tier / Indie / Pro) | $0 (use AGPLv3) | Publish source under AGPL-compatible terms; keep notices | Open-source option is **AGPLv3, not GPLv3** (compatible, but combined work carries AGPL terms); no splash screen in JUCE 8; commercial tiers have revenue limits (Starter free tier limit reported $20k–$50k — unverified, juce.com blocked; irrelevant on AGPL path) |
| Tracktion Engine | Dual: **GPLv3 (or later)** or commercial (Personal/Indie/Enterprise) | $0 (use GPLv3) | Publish source; keep notices | Personal commercial tier has $50k revenue cap — irrelevant on GPL path; engine pins a specific JUCE version (submodule); repo contains third-party code with own licenses |
| VST3 SDK (hosting) | **MIT** since VST 3.8 (Oct 29, 2025); previously GPLv3/proprietary dual | $0 | Keep MIT copyright + license text | "VST" word/logo are Steinberg trademarks — usage optional under MIT, but if used must follow Steinberg's trademark guidelines; VST2 hosting remains off-limits (licensing discontinued) |
| Audio Unit (AU) hosting | macOS system framework (`AudioToolbox`/`AudioUnit`) — **no separate SDK license** | $0 | None (system API) | macOS-native plugin format; JUCE hosts it via `AudioUnitPluginFormat`. Worth enabling on Mac since many vendors ship AU-only or AU-first; "Audio Units" is an Apple term, factual use is fine |
| CLAP | **MIT** (verified, © 2021 Alexandre Bique) | $0 | Keep MIT notice | None |
| CoreAudio (audio I/O) | macOS system framework (`CoreAudio`/`AudioToolbox`) — **no SDK, no license** | $0 | None (system API) | Native low-latency on macOS; built into JUCE `juce_audio_devices`. No SDK to vendor, no signed agreement, no GPL conflict |
| Rubber Band | Dual: **GPLv2-or-later** or commercial | $0 (GPL) | Source disclosure; keep notices | GPLv2+ upgrades cleanly into a GPLv3 project; commercial license needed only if ever going closed-source |
| signalsmith-stretch | **MIT** (verified, © 2022 Signalsmith Audio) | $0 | Keep MIT notice | None |
| libsndfile | **LGPL-2.1** (project offers 2.1 or 3.0) | $0 | LGPL: provide source/relink ability (moot — whole app is GPL); keep notices | None for a GPL app |
| FFmpeg | **LGPL-2.1+ by default**; GPLv2+ with `--enable-gpl`; `--enable-version3` for (L)GPLv3 | $0 | Ship/point to FFmpeg source for the exact build; document configure flags | Never use `--enable-nonfree` (e.g. FDK-AAC) — makes binaries unredistributable; in a GPLv3 app, `--enable-gpl --enable-version3` is fine and unlocks extra filters |
| JUCE built-in codecs (WAV/AIFF/FLAC/Ogg; MP3/AAC via macOS Core Audio) | Part of JUCE (AGPLv3 path); FLAC/Ogg are BSD-licensed libs bundled in JUCE | $0 | Keep bundled-library notices (JUCE ships them) | MP3 patents all expired (last US patent Apr 16, 2017; Fraunhofer/Technicolor program terminated Apr 23, 2017) — JUCE's MP3 reader is patent-clear; AAC *encoding* patents still active, but using the OS-provided Core Audio codec keeps you clear |
| ONNX Runtime | **MIT** (verified, © Microsoft) | $0 | Keep MIT notice | Large binary; check licenses of any execution providers you add (CPU EP is MIT; the macOS-relevant CoreML EP is MIT; vendor EPs vary) |
| LAION-CLAP weights | GitHub code+checkpoints: **CC0-1.0** (verified); HF `laion/clap-htsat-(un)fused`: **Apache-2.0** tag | $0 | CC0: none; Apache-2.0: keep notice | Training data (LAION-Audio-630K incl. Freesound, BBC SFX) has mixed CC provenance incl. NC material — unsettled area legally, low practical risk for personal embedding/search use; successor MS-CLAP is MIT; GLAP (Xiaomi, 2025) license unverified |
| Freesound API | ToS contract (not a software license); sounds: **CC0 / CC-BY 4.0 / CC-BY-NC 4.0** | $0 for **non-commercial** use | Credit Freesound + uploader per sound license; respect rate limits (60/min, 2000/day); one API key; OAuth2 for full-quality downloads | **Do not replicate Freesound** (no redistributable mirror of sounds/metadata); CC-BY-NC sounds taint commercial use of user projects; open-source app can't embed a secret API key — have users supply their own |

**(a) GPLv3-incompatibility flags:** none. The audio path (CoreAudio) and the plugin paths (VST3 MIT, AU system framework, CLAP MIT) are all clean. JUCE's AGPLv3 is compatible but "upgrades" the effective license of distributed builds.
**(b) Storage/redistribution-of-sounds flags:** Freesound only — see detail section; you may store sounds locally for the user, but must not rebuild/redistribute Freesound's collection, and CC-BY/CC-BY-NC obligations follow each sound into user projects.

---

## Per-item detail

### 1. JUCE 8

- **Verified (primary):** `JUCE/LICENSE.md` (master, fetched 2026-06-11): JUCE Framework modules are dual-licensed **AGPLv3 or the JUCE 8 EULA**. Examples are ISC; bundled dependencies carry Apache-2.0/BSD/MIT/zlib/ISC and proprietary licenses for **AAX**. Source: https://github.com/juce-framework/JUCE/blob/master/LICENSE.md
- **Verified (primary):** `CHANGE_LIST.md` for 8.0.11: VST3 SDK updated to 3.8.0 (MIT). Source: https://github.com/juce-framework/JUCE/blob/master/CHANGE_LIST.md
- **Corroborated (secondary):** JUCE 8 (released June 12, 2024) **dropped the splash-screen requirement** for the free tier — multiple forum/community sources (e.g. https://forum.juce.com/t/juce-8-is-available-now/61809 ; HISE forum threads). The old "Made with JUCE" splash was a JUCE 5–7 Personal-tier mechanism; under AGPLv3 there was never a splash requirement.
- **Unverified / flagged:** exact free-tier (Starter) revenue limit. juce.com returned 403 to automated fetch. Community reports conflict: $20k/yr (HISE forum, 2024-25) vs the older Personal tier's $50k (JUCE 7 EULA-era forum posts). **Irrelevant for this project** if you take the AGPLv3 path, which has no revenue limit. Check https://juce.com/get-juce/ and https://juce.com/legal/juce-8-licence/ manually if the commercial tier ever matters.
- **Hurdle — AGPL vs GPLv3:** You cannot use JUCE 8 under plain GPLv3. AGPLv3 and GPLv3 are one-way compatible via GPLv3 §13: your code can stay GPLv3(-or-later) and link with AGPLv3 JUCE, but the conveyed combination includes AGPLv3-governed parts. Practical effect for a desktop DAW is near-zero (the AGPL network clause only bites for network-server use), but state it correctly in your README/COPYING: e.g. "Project code: GPL-3.0-or-later. Builds incorporate JUCE under AGPL-3.0."
- **Gotchas:** the JUCE EULA (non-AGPL path) counts *gross* revenue across all uses; AAX/closed-platform targets can't be built on the pure-GPL/AGPL path (not relevant here — no AAX in a GPL DAW anyway).

### 2. Tracktion Engine

- **Verified (primary):** `tracktion_engine/LICENSE.md` (develop, fetched 2026-06-11): *"dual GPL3 (or later)/Commercial license"*, multiple commercial tiers, and an explicit warning that the repo contains third-party open-source code you must also comply with. Source: https://github.com/Tracktion/tracktion_engine/blob/develop/LICENSE.md
- **Corroborated (official page, via search snippets — page itself 403'd):** Tracktion Engine **Personal** (commercial tier) carries a **$50,000 gross revenue limit**; above that you must upgrade to Indie/Enterprise **or release under GPL**. Source: https://engine.tracktion.com/agreement (snippet retrieved 2026-06-11).
- **Cost for this project:** $0 under GPLv3-or-later. No signup, no agreement, no revenue accounting.
- **Obligations:** standard GPLv3 — full corresponding source published, license text and notices retained.
- **Gotchas / hurdles:** the engine pins JUCE as a submodule; mixing your own JUCE version can break builds — track the engine's JUCE. "GPL3 or later" combines cleanly with AGPLv3 JUCE. Watch the `modules/3rd_party` content (each piece has its own license; all open-source per LICENSE.md, but verify when you vendor).

### 3. Steinberg VST3 SDK (hosting)

- **Verified (primary):** `steinbergmedia/vst3sdk` `LICENSE.txt` (master, fetched 2026-06-11) is the **MIT license**, "Copyright (c) 2025, Steinberg Media Technologies GmbH". Source: https://github.com/steinbergmedia/vst3sdk/blob/master/LICENSE.txt
- **Dates:** Steinberg announced the change with VST 3.8 in a press release dated **October 29, 2025** (press PDF: https://ocl-steinberg-live.steinberg.net/_storage/asset/819253/storage/master/Press%20Release%20-%202025-10-29%20-%20VST%203.8%20-%20EN.pdf ; coverage: https://www.kvraudio.com/news/steinberg-moves-vst-3-sdk-to-mit-open-source-license-asio-now-gplv3-65179 , https://librearts.org/2025/11/steinberg-relicenses-vst3-and-asio/ , 2025-11). Before this, the SDK was dual GPLv3/proprietary, and GPLv3 hosting was already fine — MIT just removes all friction.
- **Hosting under GPLv3:** MIT code embeds into a GPLv3 work with no conflict. Obligation: retain the MIT copyright/license text in source and About/credits. No source-disclosure obligation arises from the SDK itself (your GPL covers that anyway). Loading third-party proprietary VST3 *plugins* at runtime is the classic GPL boundary question; consensus and universal practice (Ardour, Qtractor, etc.) treat plugins as separate works communicating via a published interface — hosting proprietary plugins in a GPL host is accepted practice. (Inference/industry practice, not case law.)
- **Trademark (verified via Steinberg dev portal, summarized):** "VST" name/logo use is **optional** under MIT, but if you display the VST logo or use the trademark in marketing you must follow Steinberg's usage guidelines (logo placement rules etc.). Plain factual statements like "hosts VST3 plug-ins" are nominative use. Sources: https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/Usage+guidelines.html , https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/VST3+License.html
- **Gotcha:** **VST2** licensing was discontinued years ago; no new VST2 host agreements exist. Don't host VST2 — VST3 + CLAP covers the modern ecosystem.

### 4. CLAP

- **Verified (primary):** `free-audio/clap` `LICENSE` (main, fetched 2026-06-11): **MIT**, "Copyright (c) 2021 Alexandre BIQUE". Source: https://github.com/free-audio/clap/blob/main/LICENSE
- Header-only C ABI; $0; keep the notice. Designed explicitly to be FOSS-host friendly (u-he/Bitwig project). No trademark program akin to VST's. **No gotchas.**

### 5. Audio I/O — CoreAudio (no SDK, no licensing surface)

- **There is no licensing question here.** The native low-latency audio API is **CoreAudio**, part of the OS. JUCE talks to it through `juce_audio_devices` (`juce::AudioIODevice` backed by `CoreAudioIODevice`); you enable it with `JUCE_USE_COREAUDIO` (on by default on macOS). No SDK to download, no signed agreement, no redistributable third-party code, no GPL conflict. CoreAudio already provides exclusive, low-latency device access with selectable buffer sizes out of the box.
- **Pro Audio / aggregate devices:** standard CoreAudio features (the system default device, user-created Aggregate Devices, and the "Pro Audio" hog/exclusive mode) are all handled by JUCE's device layer — no extra licensing surface.

### 6. Rubber Band Library

- **Verified (primary):** `breakfastquay/rubberband` `COPYING` (default branch, fetched 2026-06-11) is the **GPLv2** text; the README and breakfastquay.com state distribution under **"GNU GPL v2 or later"**, with a paid commercial license alternative (no royalties, no expiry; pricing not published in fetched sources — breakfastquay.com 403'd). Sources: https://github.com/breakfastquay/rubberband , https://breakfastquay.com/rubberband/license.html
- **For this project:** "GPLv2 or later" upgrades to GPLv3 — fully compatible, $0. Obligations: GPL source disclosure (already covered), notices.
- **Hurdle (minor):** if you ever wanted a non-GPL build, Rubber Band needs a commercial license — but signalsmith-stretch (MIT) is your hedge for time-stretching.

### 7. signalsmith-stretch

- **Verified (primary):** `LICENSE.txt` (main, fetched 2026-06-11): **MIT**, "Copyright (c) 2022 Geraint Luff / Signalsmith Audio Ltd." Source: https://github.com/Signalsmith-Audio/signalsmith-stretch
- $0, keep notice. **No gotchas.** Good MIT-licensed alternative/complement to Rubber Band.

### 8. Audio codec path

- **libsndfile — verified (primary):** `COPYING` (master, fetched 2026-06-11) is **LGPL-2.1**; the project documents an option of LGPL 2.1 **or** 3.0. Either is GPLv3-compatible. In a GPLv3 app the LGPL relinking provisions are trivially satisfied by the app being open. Source: https://github.com/libsndfile/libsndfile/blob/master/COPYING
- **FFmpeg — verified (primary):** `LICENSE.md` (master, fetched 2026-06-11): default **LGPL-2.1+**; `--enable-gpl` switches the build to **GPLv2+** and unlocks GPL-only components (x264/x265 wrappers, ~29 GPL filters incl. delogo/boxblur, etc.); `--enable-version3` moves to (L)GPLv3 for Apache-2.0-dependency compatibility; `--enable-nonfree` (FDK-AAC, etc.) makes binaries **unredistributable — never use it**. For a GPLv3 DAW: `--enable-gpl --enable-version3` is clean; document your exact configure line and provide the FFmpeg source for the build you ship. Source: https://github.com/FFmpeg/FFmpeg/blob/master/LICENSE.md
  - *Hurdle note:* honestly consider whether you need FFmpeg at all — libsndfile + JUCE/Core Audio covers WAV/AIFF/FLAC/Ogg/MP3/AAC-decode. FFmpeg adds a big compliance/document surface (and codec patent surface for *encoders*) for marginal gain in a DAW.
- **JUCE built-in formats:** WAV/AIFF readers are JUCE code (AGPLv3 path); FLAC and Ogg Vorbis come from the bundled Xiph libraries (BSD-style, notices ship with JUCE); on macOS `juce::CoreAudioFormat` uses the OS **Core Audio / AudioToolbox** codecs for MP3/AAC (and other system-supported formats) decode — the codec lives in the OS, so no patent or license obligation lands on you (standard, low-risk pattern).
- **MP3 patents — verified:** the last US MP3 patent (US 6,009,399) expired **April 16, 2017**; Fraunhofer/Technicolor **terminated the mp3 licensing program April 23, 2017**. MP3 encode and decode are patent-free worldwide. Sources: https://www.iis.fraunhofer.de/en/ff/amm/consumer-electronics/mp3.html , https://www.theregister.com/2017/05/16/mp3_dies_nobody_noticed/ , https://www.macrumors.com/2017/05/15/mp3-format-terminated/ (all reporting 2017 events).
- **AAC:** decoding via OS-provided Core Audio = fine. Shipping your *own* AAC **encoder** (e.g. FDK-AAC) raises both patent (Via LA pool still active) and GPL-incompatibility (FDK license) issues — avoid; use the macOS system AAC encoder (Core Audio / AudioToolbox `AudioConverter`) if AAC export is ever needed.

### 9. ONNX Runtime

- **Verified (primary):** `microsoft/onnxruntime` `LICENSE` (main, fetched 2026-06-11): **MIT**, "Copyright (c) Microsoft Corporation". Source: https://github.com/microsoft/onnxruntime/blob/main/LICENSE
- $0; keep notice. MIT embeds fine in a GPLv3 app. **Gotchas:** execution providers vary — the **CoreML EP** (the macOS GPU/Neural-Engine accelerator) is MIT and uses Apple's system CoreML framework. For **CPU EP + optional CoreML EP** you're clean. (Note: a CLAP text encoder is tiny — CPU EP alone is fine; CoreML is only worth it for the heavier audio tower.)

### 10. LAION-CLAP weights (and successors)

- **Verified (primary):** `LAION-AI/CLAP` GitHub `LICENSE` (main, fetched 2026-06-11): **CC0-1.0** — covers the code and, per the repo, its released checkpoints. Source: https://github.com/LAION-AI/CLAP
- **Verified (search-level, HF page 403'd):** Hugging Face `laion/clap-htsat-fused` / `clap-htsat-unfused` are tagged **apache-2.0**; `lukewys/laion_clap` (the canonical checkpoint mirror) is tagged **CC0-1.0**. The `laion/larger_clap_music`, `larger_clap_general`, `larger_clap_music_and_speech` cards' license tags were **not directly verifiable** during this audit (403) — *flagged; check the model card before committing to a specific checkpoint*. Sources: https://huggingface.co/laion/clap-htsat-fused , https://huggingface.co/lukewys/laion_clap
- **Training-data provenance (inference, flagged):** LAION-Audio-630K includes Freesound (incl. CC-BY and CC-BY-NC sounds), BBC Sound Effects, and other scraped sources. Whether model weights are derivative works of training audio is legally unsettled (no controlling precedent as of mid-2026). Practical exposure for a personal, non-commercial DAW using the weights only for **embedding/search** (not generation) is very low; the weights' CC0/Apache grant is what you act on. Don't redistribute training audio; redistributing the weights themselves is permitted by their licenses.
- **Successors:** **Microsoft CLAP (msclap, 2022/2023 checkpoints)** — code and weights **MIT** ( https://github.com/microsoft/CLAP , https://huggingface.co/microsoft/msclap ) — a clean fallback. **GLAP** (Xiaomi Research, 2025; multilingual speech/sound/music) looks technically attractive but its **weights license was not verifiable** in this audit ( https://github.com/xiaomi-research/dasheng-glap , https://huggingface.co/mispeech/GLAP ) — *verify before adopting*.

### 11. Freesound API ToS

Primary text (API Terms of Use, from the official `MTG/freesound` repo `_docs/api/source/terms_of_use.rst`, fetched 2026-06-11; mirrors https://freesound.org/help/tos_api/ which 403'd):

- **Commercial use:** *"You can use the Freesound API for free only for non-commercial purposes."* Commercial use requires contacting MTG (mtg@upf.edu). A personal, non-commercial GPL DAW qualifies for free use. *Hurdle:* if this ever becomes donation-funded or sold, the API terms need renegotiation.
- **No replication:** *"Do not use the api to replicate Freesound in another site or to present Freesound data pretending it is yours."* → **This is the storage/redistribution flag.** A local cache of previews/metadata for the user's own browsing and offline projects is normal client behavior; building a redistributable mirror, bulk-scraping the catalog, or shipping a prebuilt index of Freesound metadata with your app would violate this. (The boundary is my inference from the clause; the ToS doesn't define "cache" explicitly — flagged.)
- **Attribution:** *"Remember to properly credit Freesound and Freesound users"* in accordance with each sound's license. For the client app: show sound author + license in the browser UI and write attribution info into the project file when a sound is imported. Recommended: an exportable "credits" list per project.
- **Rate limits (ToS-level obligation, verified from official API docs `overview.rst`):** **60 requests/min and 2,000 requests/day** standard; write-type endpoints 30/min, 500/day; HTTP 429 on throttle. *"Do not register multiple API keys to circumvent request limitations."* Higher limits by request to MTG.
- **Auth (verified from official docs):** Token (API key) auth suffices for search, metadata, and **previews** (auto-generated .mp3/.ogg at lower quality). **OAuth2 is required to download original-quality files** (and for uploads/ratings). So a useful client needs the full OAuth2 authorization-code flow with the user's own Freesound account.
- **Sound licenses on Freesound (current set):** **CC0-1.0**, **CC-BY 4.0**, **CC-BY-NC 4.0** (legacy uploads may carry CC-BY/NC 3.0 and the retired **Sampling+** — Freesound advises treating Sampling+ like CC-BY-NC). Obligations when sounds land in user projects:
  - **CC0:** no obligations, ever.
  - **CC-BY:** if the user distributes/publishes work containing the sound (a rendered track counts), they must credit the author, name the license, link it, and note changes. Your DAW should preserve and surface this metadata.
  - **CC-BY-NC:** same, plus **no commercial use** of the resulting work. *Hurdle:* a user who renders a CC-BY-NC sample into a track cannot monetize that track. Strongly consider a UI filter/warning (Freesound's API supports filtering by license) — defaulting searches to CC0+CC-BY avoids poisoning user projects.
- **API key in an open-source client (practical hurdle, inference):** API credentials can't be kept secret in GPL source. Standard practice (used by other FOSS Freesound clients) is to require each user to register their own API key/OAuth app at https://freesound.org/apiv2/apply — document this in setup. Per-user keys also keep each user inside their own rate limit.

Sources: https://github.com/MTG/freesound/blob/master/_docs/api/source/terms_of_use.rst , https://github.com/MTG/freesound/blob/master/_docs/api/source/overview.rst , https://freesound.org/docs/api/ , https://freesound.org/help/tos_api/ , https://freesound.org/help/faq/ (all current as of 2026-06-11).

---

## Hurdles & action list

1. **License header decision (do first):** project = **GPL-3.0-or-later**; document that distributed builds incorporate JUCE (AGPL-3.0), Tracktion Engine (GPL-3.0-or-later), VST3 SDK + CLAP + signalsmith-stretch + ONNX Runtime (MIT), Rubber Band (GPL-2.0-or-later), libsndfile (LGPL-2.1). CoreAudio and Audio Unit hosting are macOS system frameworks (no third-party license). Keep a `THIRD_PARTY_LICENSES` file.
2. **Use JUCE ≥ 8.0.11** for the MIT VST3 SDK 3.8 bundle. Audio I/O is CoreAudio (a system framework) — nothing to vendor or license.
3. **Freesound compliance design:** OAuth2 flow for originals; per-user API key; attribution metadata stored in project files + exportable credits; license filter defaulting away from CC-BY-NC; local caching limited to user-driven content (no catalog mirroring).
4. **Codec path:** prefer libsndfile + JUCE/Core Audio; only add FFmpeg if a concrete need appears (then `--enable-gpl --enable-version3`, never `--enable-nonfree`).
5. **Verify manually (couldn't be fetched programmatically):** exact JUCE 8 Starter revenue limit (juce.com), Tracktion Engine agreement full text (engine.tracktion.com/agreement), HF license tags for `laion/larger_clap_*`, GLAP weights license. None of these block the GPL path.

## Source index (all accessed 2026-06-11)

- JUCE LICENSE.md — https://github.com/juce-framework/JUCE/blob/master/LICENSE.md
- JUCE CHANGE_LIST.md (8.0.11 VST3 3.8 entry) — https://github.com/juce-framework/JUCE/blob/master/CHANGE_LIST.md
- JUCE 8 EULA / Get JUCE (403 to bots; verify manually) — https://juce.com/legal/juce-8-licence/ , https://juce.com/get-juce/
- JUCE 8 release thread (Jun 12, 2024) — https://forum.juce.com/t/juce-8-is-available-now/61809
- Tracktion Engine LICENSE.md — https://github.com/Tracktion/tracktion_engine/blob/develop/LICENSE.md
- Tracktion Engine EULA — https://engine.tracktion.com/agreement
- VST3 SDK MIT LICENSE.txt — https://github.com/steinbergmedia/vst3sdk/blob/master/LICENSE.txt
- Steinberg VST 3.8 press release (2025-10-29) — https://ocl-steinberg-live.steinberg.net/_storage/asset/819253/storage/master/Press%20Release%20-%202025-10-29%20-%20VST%203.8%20-%20EN.pdf
- VST trademark/usage guidelines — https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/Usage+guidelines.html
- KVR: "Steinberg Moves VST 3 SDK to MIT" (Oct 2025) — https://www.kvraudio.com/news/steinberg-moves-vst-3-sdk-to-mit-open-source-license-asio-now-gplv3-65179
- CLAP LICENSE — https://github.com/free-audio/clap/blob/main/LICENSE
- Rubber Band license page / repo — https://breakfastquay.com/rubberband/license.html , https://github.com/breakfastquay/rubberband
- signalsmith-stretch LICENSE — https://github.com/Signalsmith-Audio/signalsmith-stretch/blob/main/LICENSE.txt
- libsndfile COPYING — https://github.com/libsndfile/libsndfile/blob/master/COPYING
- FFmpeg LICENSE.md — https://github.com/FFmpeg/FFmpeg/blob/master/LICENSE.md
- Fraunhofer mp3 page / patent expiry coverage (2017) — https://www.iis.fraunhofer.de/en/ff/amm/consumer-electronics/mp3.html , https://www.theregister.com/2017/05/16/mp3_dies_nobody_noticed/
- ONNX Runtime LICENSE — https://github.com/microsoft/onnxruntime/blob/main/LICENSE
- LAION-CLAP repo (CC0-1.0) — https://github.com/LAION-AI/CLAP
- LAION CLAP HF cards — https://huggingface.co/laion/clap-htsat-fused , https://huggingface.co/lukewys/laion_clap
- Microsoft CLAP (MIT) — https://github.com/microsoft/CLAP , https://huggingface.co/microsoft/msclap
- GLAP — https://github.com/xiaomi-research/dasheng-glap , https://huggingface.co/mispeech/GLAP
- Freesound API ToS source — https://github.com/MTG/freesound/blob/master/_docs/api/source/terms_of_use.rst
- Freesound API overview (auth, rate limits, previews vs originals) — https://github.com/MTG/freesound/blob/master/_docs/api/source/overview.rst , https://freesound.org/docs/api/
- Freesound FAQ (sound licenses, Sampling+ guidance) — https://freesound.org/help/faq/
