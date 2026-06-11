# 03 — Integrated Online Sound Discovery: Freesound API, Semantic Search, and Architecture

**Research date:** 2026-06-11
**Scope:** Freesound API deep-dive, other legally API-accessible sound sources, semantic (text→audio) search state of the art, local index tooling, inference integration for a C++/JUCE/Tracktion DAW on macOS, GPLv3.

**Sourcing note / verification caveat:** The research sandbox could not reach `freesound.org` directly (proxy allowlist). All Freesound API facts below were instead verified against the **primary source of the docs**: the reStructuredText sources in the official `MTG/freesound` GitHub repository (`_docs/api/source/*.rst`, `master` branch, fetched 2026-06-11 from raw.githubusercontent.com). These are the files that render to https://freesound.org/docs/api/. The master-branch docs contain a note dated **November 2025** (deprecation of the old text-search endpoint), so they are current. There is a small residual risk that `master` is slightly ahead of what is deployed on freesound.org — re-verify the handful of items marked ⚠ against the live docs/browseable API (https://freesound.org/apiv2) before coding against them.

---

## 1. Freesound API in depth

Docs: https://freesound.org/docs/api/ (source: https://github.com/MTG/freesound/tree/master/_docs/api/source)

### 1.1 Authentication: token vs OAuth2 (VERIFIED, primary source)

Two strategies (`authentication.rst`):

| | Token auth | OAuth2 (authorization-code grant, RFC 6749) |
|---|---|---|
| How | `?token=API_KEY` or header `Authorization: Token API_KEY` | `Authorization: Bearer ACCESS_TOKEN`, must be HTTPS |
| Get credentials | https://freesound.org/apiv2/apply | Same page (client_id + client_secret + redirect URL) |
| Search, sound metadata, similarity, analysis | ✅ | ✅ |
| **Preview downloads (mp3/ogg)** | ✅ **"Retrieving previews does not require OAuth2 authentication"** (`overview.rst`) | ✅ |
| **Original-quality download** (`GET /apiv2/sounds/<id>/download/`) | ❌ | ✅ **"OAuth2 required"** (`resources.rst`) |
| Upload / describe / rate / comment / bookmark / `me` | ❌ | ✅ |

So the hypothesis is **confirmed**: full-quality original files (wav/aif/flac/ogg/mp3, format = whatever was uploaded) require OAuth2; previews work with plain token auth.

OAuth2 flow details (all verified):
- Authorize URL: `https://freesound.org/apiv2/oauth2/authorize/?client_id=...&response_type=code&state=...` (also `logout_and_authorize/` variant to force re-login).
- Token URL: `POST https://freesound.org/apiv2/oauth2/access_token/` with `client_id`, `client_secret`, `grant_type=authorization_code`, `code`.
- **Authorization codes live 10 minutes, single-use.**
- **Access tokens live 24 hours** (`expires_in: 86399`); a `refresh_token` is issued; refresh with `grant_type=refresh_token`. Only one access token per app/user pair (new one overwrites old).
- **Desktop-app-friendly out-of-band option (verified):** if your app "can't handle requests", Freesound itself can be set as the redirect target; the user then sees the authorization `code` printed on a Freesound page and pastes it into your app manually. This is an officially supported fallback — no embedded web server strictly required.
- The standard desktop pattern (inference, not Freesound-specific docs): register `http://localhost:<port>/callback` as the redirect URL, open the system browser, run a tiny loopback HTTP listener (RFC 8252 "OAuth 2.0 for Native Apps"). Freesound's redirect URL is fixed at credential-creation time; whether it accepts loopback URLs with variable ports is **unverified — test it**; the copy-paste fallback exists either way.
- Note: there is **no PKCE mention** in the docs; the flow uses `client_secret`, which cannot be kept secret in a GPLv3 desktop binary. This is a known, generally tolerated weakness of desktop OAuth (the secret protects little when previews/search don't need OAuth anyway). Flagged in Hurdles.

### 1.2 Search endpoint, filters, sorting, pagination (VERIFIED, primary source)

**Endpoint:** `GET /apiv2/search/` — note this **replaces the old `/apiv2/search/text/` endpoint, deprecated November 2025** (old URL currently redirects). Use the new one.

Parameters (`resources.rst`):

- `query` — free text; terms space-separated; phrases in `"..."`; `+`/`-` modifiers (`query=bass -drum`). Weighted lexical (Solr) search over tags, name, description, pack name, sound id. Default weights ≈ `id:4,tag:4,description:3,original_filename:2,username:2,pack:2` (overridable via `weights` param). Empty query returns all sounds.
- `filter` — Solr filter syntax (below).
- `sort` — one of `score` (default), `duration_desc/asc`, `created_desc/asc`, `downloads_desc/asc`, `rating_desc/asc`; **or** a numeric sorting target: `sort=pitch:220,pitch_var:0.0` (euclidean-distance sort on numeric descriptor fields).
- `similar_to` + `similarity_space` — built-in similarity search, see §1.3. **This is the headline feature.**
- `group_by_pack=1` — collapse same-pack results (adds `n_from_same_pack`, `more_from_same_pack`).
- `fields` — comma-separated list of sound fields to return per result (default `id,name,tags,username,license`; plus pseudo-field `score`). **Critical for efficiency: ask for `id,name,username,license,duration,previews,images,tags,filesize,type,samplerate` in one request and you never need per-sound follow-up requests.**
- `page` (default 1), `page_size` (default 15, **max 150**). Response: `{count, next, previous, results:[...]}` with `next`/`previous` page links.

**Filter syntax** (Solr): `filter=fieldname:value`, quoted multi-word values, ranges `duration:[0.1 TO 0.3]` (`TO` uppercase, `*` wildcard for open end), dates `created:[NOW-1YEAR/DAY TO NOW]`, boolean logic `type:(wav OR aiff)`, `description:(piano AND note)`. Note **`tags` and `comments` filter as singular `tag` / `comment`**. Geo filters exist (`{!geofilt sfield=geotag pt=lat,lon d=km}`, `geotag:"Intersects(...)"`).

Filterable fields = everything marked filterable in the sound-instance table: `id, name, tag, description, category, subcategory, is_geotagged, created, license, ai_preference, type, channels, filesize, bitrate, bitdepth, duration, samplerate, username, md5, is_remix, was_remixed, pack, num_downloads, avg_rating, num_ratings, num_comments` **plus ~50 content-based descriptors**: `bpm, note_name, note_midi, note_confidence, pitch, pitch_confidence…, tonality ("C minor"), loopable (bool!), single_event (bool), loudness (LUFS, EBU R128), dynamic_range, log_attack_time, brightness, warmth, hardness, depth, boominess, roughness, sharpness, spectral_centroid, duration_effective, onset_count, reverbness…`

Examples straight from the official examples file (`apiv2/examples.py`):

```
/apiv2/search/?query=music&filter=samplerate:44100 type:wav channels:2
/apiv2/search/?query=music&filter=duration:[0.1 TO 0.3] avg_rating:[3 TO *]
/apiv2/search/?query=piano&filter=bpm:60
/apiv2/search/?query=piano&filter=note_name:E&filter=note_confidence:[0.9 TO *]
/apiv2/search/?query=alarm&fields=name,previews
/apiv2/search/?query=music&filter=category:Music subcategory:"Solo instrument"
```

Hugely useful for a DAW: `filter=loopable:true`, `filter=single_event:true` (one-shots!), `bpm`, `tonality`, `note_name` — Freesound precomputes the exact musical metadata a DAW browser wants (tempo-sync, key-tag, loop vs one-shot), via Essentia + AudioCommons descriptors. There is also a **Broad Sound Taxonomy** `category`/`subcategory` (auto-filled by an algorithm when uploader didn't set it), e.g. "Instrument samples" / "Piano / Keyboard instruments".

### 1.3 Built-in similarity / semantic search — **Freesound already runs LAION-CLAP** (VERIFIED, primary source)

This is the single most decision-relevant finding. From `resources.rst` (master, post-Nov-2025):

- `similar_to=<sound_id>` **or** `similar_to=[1.36, 2.05, ...]` (a raw float vector!) sorts results by similarity.
- `similarity_space` selects the embedding space:

| Space | Dims | What it is |
|---|---|---|
| `laion_clap` | **512** | "Built using LAION-CLAP embeddings… L2-normed versions… extracted using the standard tools provided by LAION (https://github.com/LAION-AI/CLAP). **We use the `630k-audioset-fusion-best.pt` pre-trained model.**" |
| `freesound_classic` | 100 | Legacy low-level Essentia `FreesoundExtractor` features (no public extractor code). |

- Input vectors are auto-L2-normalized. There is also `GET /apiv2/sounds/<id>/similar/` (same thing, ID in URI) and `GET /apiv2/sounds/<id>/analysis/` which returns **the stored similarity vectors** for a sound — i.e. you can pull Freesound's own CLAP embeddings for any sound.
- `similar_to` is a parameter of `/apiv2/search/`, so it **composes with `filter`** (e.g. CLAP-similarity + `filter=license:("Attribution" OR "Creative Commons 0") duration:[* TO 10]`).

**The semantic-search unlock (inference, high confidence, must prototype):** because LAION-CLAP is a *joint* audio-text space, you can run **only the CLAP text encoder locally**, embed the user's natural-language query ("dusty vinyl crackle loop, 90 bpm"), and pass that 512-dim vector as `similar_to=[...]` with `similarity_space=laion_clap`. Freesound's server does the nearest-neighbor search over its ~700k+ sounds. The docs only describe extracting vectors "for a sound", and do not explicitly bless text-embedding input — but the vectors live in the same space, the API L2-normalizes for you, and text→audio retrieval is exactly what CLAP was trained for. The checkpoint must match: **`630k-audioset-fusion-best.pt`** (the `enable_fusion=True` LAION-CLAP model = HF `laion/clap-htsat-fused`). Verify empirically early (a day of prototyping in Python decides this). Caveats: undocumented usage could change; behavior of `query` combined with `similar_to` (does lexical query still gate results?) is unspecified — test.

Even if text-vector input underperforms, plain `similar_to=<id>` ("find me more like this one") is documented, free, and a killer DAW feature on its own.

### 1.4 Previews (VERIFIED)

`previews` object on every sound (available in search results via `fields=previews`):

- `preview-hq-mp3` — ~**128 kbps** mp3
- `preview-lq-mp3` — ~**64 kbps** mp3
- `preview-hq-ogg` — ~**192 kbps** ogg
- `preview-lq-ogg` — ~**80 kbps** ogg

Plus `images`: `waveform_l/m`, `spectral_l/m` (pre-rendered waveform & spectrogram PNGs — free UI candy for the result list). Previews download with token auth (or, in practice, the URLs are plain static URLs). HQ-ogg at ~192 kbps is fine for audition and acceptable as a scratch clip on the timeline; it is **not** release-quality for tonal material — see Hurdles §8.5 and the replace-with-original flow in §9.

### 1.5 Rate limits (VERIFIED, primary source)

From `overview.rst` ("Throttling"):
- **Standard: 60 requests/minute and 2,000 requests/day** per API credential.
- Write-ish resources (upload, describe, comment, rate, bookmark): **30/min, 500/day**.
- Exceeding → `429 Too many requests` with `detail` saying which limit tripped.
- Higher limits: **email mtg@upf.edu** ("if these usage limits are not enough… contact Freesound administrators"). No self-serve tier table exists anymore; older "approved key" tiering language is gone from current docs.
- ToS explicitly forbids registering multiple API keys to dodge limits.

Practical math: 2,000 req/day is plenty for one user's interactive searching (a search = 1 request thanks to `fields`), but **the limit is per API key, not per end-user**. If you ship one embedded key to many users, the whole user base shares 2,000/day. Mitigations: have each user paste their own free API key (one-time setup, like many Freesound-integrated tools), or get blessed higher limits from MTG. For a personal/solo DAW this is a non-issue.

### 1.6 Licenses, attribution, ToS (VERIFIED primary source + dated secondary)

- API ToS (`terms_of_use.rst`, https://freesound.org/help/tos_api/): **free for non-commercial use only**; commercial API use requires contacting mtg@upf.edu for licensing. "Do not replicate Freesound in another site"; **"Remember to properly credit Freesound and Freesound users in accordance to sounds' licenses"**; don't abuse bandwidth.
- `license` field values (current, verified): **"Attribution"** (CC-BY), **"Attribution NonCommercial"** (CC-BY-NC), **"Creative Commons 0"** (CC0). These are also the only values accepted at upload. (License *versions* 3.0/4.0 are not distinguished in the filter value; the per-sound `license` field returns the deed URL when fetched as metadata — ⚠ verify exact returned shape at runtime.)
- **Filter syntax for licenses** (the thing the DAW needs):
  - `filter=license:"Creative Commons 0"` — CC0 only
  - `filter=license:("Attribution" OR "Creative Commons 0")` — **the safe default for music-making: excludes NC**
- **Legacy Sampling+:** Creative Commons retired Sampling+ in 2011 and Freesound 2.0 stopped offering it (CC blog, 2011-09-12: https://creativecommons.org/2011/09/12/celebrating-freesound-2-0-retiring-sampling-licenses/). Old sounds keep it until the uploader relicenses, so **Sampling+-licensed sounds can still surface**. The API's documented value list doesn't include it; treat any unrecognized license string conservatively (hide or warn). ⚠ Verify what the API returns for a pre-2011 Sampling+ sound.
- New in 2025-era API: **`ai_preference`** field/filter — uploader's preference about generative-AI training use (`freesound-cc-recommendation`, `open-models`, `open-noncommercial-models`). Not legally binding for playback/sampling use, but if you ever build a local embedding index or train anything, filtering on this is the polite (and increasingly expected) move.
- Attribution mechanics: store `username`, sound `url`, sound `name`, and `license` with every imported clip; CC-BY 4.0 requires title/author/source/license indication. A DAW that auto-generates a "credits.txt" per project satisfies this trivially. Precedent: Soundly's Freesound add-on and Acoustica **Mixcraft's built-in "Freesound.org button"** (https://acoustica.com/mixcraft-10-manual/freesound-org-button) — DAW-embedded Freesound search is established practice, not a ToS gray zone.

### 1.7 Existing client code

- **`MTG/freesound-juce`** — official JUCE C++ client for the Freesound API (https://github.com/MTG/freesound-juce). Token + OAuth2 support, maps API JSON to C++ objects (`FreesoundClient`, `Sound`, `SoundList`, `Pack`). Two files (`FreesoundAPI.h/.cpp`) you copy into the project. It predates the 2025 search-endpoint changes (⚠ check it still targets `/apiv2/search/` vs deprecated `/search/text/`; the redirect keeps it working either way), but as a GPL-compatible starting point inside a JUCE DAW it's nearly tailor-made. Fork and modernize rather than depend.
- `freesound-python` (MTG) — handy for offline experiments/prototyping the text-vector trick.

---

## 2. Other legally API-accessible sources (as of June 2026)

### Sanctioned / usable

| Source | Access | Licensing | Verdict for the DAW |
|---|---|---|---|
| **Jamendo** (music tracks, ~600k+) | Real REST API v3.0, https://developer.jamendo.com/v3.0, OAuth2 for user features; API **free for non-commercial use**, commercial use needs a quote (https://devportal.jamendo.com/api_terms_of_use) | All tracks CC-licensed, license indicated per track (lots of NC/ND — must filter) | Good for full songs/stems discovery; less useful for samples. Phase 2. |
| **Internet Archive** (audio mediatype, millions of items) | Official APIs: `https://archive.org/advancedsearch.php` (Lucene-ish queries, JSON) + metadata API; docs https://archive.org/services/docs/api/ | Wildly mixed; `licenseurl` field per item, e.g. `mediatype:(audio) AND licenseurl:*creativecommons*` or `licenseurl:*publicdomain*`; **many items have no license metadata at all** | Sanctioned and free; quality/metadata is chaotic. Good optional source with hard license-filtering. |
| **Wikimedia Commons** | MediaWiki Action API (no auth for read), e.g. `action=query&generator=search&gsrsearch=filetype:audio thunder&prop=imageinfo&iiprop=url|extmetadata`; https://commons.wikimedia.org/wiki/Commons:API | Everything is free-licensed by policy (CC0/CC-BY/CC-BY-SA/PD); license in `extmetadata.LicenseShortName/LicenseUrl` | Sanctioned. Mostly field recordings, speech, instruments; **beware CC-BY-SA share-alike for music use** (SA on a sample arguably propagates to the derived work — flag/exclude SA by default). |
| **Openverse** (WordPress Foundation aggregator) | Proper REST API: `https://api.openverse.org/v1/audio/?q=...&license=cc0,by` — aggregates **Freesound, Jamendo, Wikimedia** with normalized license metadata | Normalized CC license fields, filterable | One API, three catalogs, normalized licensing. Great as a secondary/unified backend; but no CLAP similarity, lexical only, and freshness lags the origin sites. |
| **BBC Sound Effects** (~33,000 sfx, WAV) | **No official public API.** Site https://sound-effects.bbcrewind.co.uk; community bulk-downloaders hit undocumented JSON endpoints (e.g. github.com/FThompson/BBCSoundDownloader) — not sanctioned | **RemArc Licence: personal/educational/research use only — NOT for music you distribute/sell; AI training & data mining excluded; commercial use requires paid BBC license** | **Exclude from the integrated browser** (license incompatible with making distributable music + no sanctioned API). At most: a "search on BBC site" link-out. |

Also exists but lower value: NASA audio (public domain, images-api.nasa.gov covers audio), Library of Congress (loc.gov JSON API, citizen-DJ-style PD collections), Musopen (classical recordings, API limited). Pixabay's official API covers images/video only — its audio library is **not** API-exposed (verify before assuming).

### Explicitly forbidden (do NOT integrate)

Verified ToS language (June 2026 search of each site's terms):
- **Splice** (splice.com/terms): prohibits "any manual or automated software, devices or other processes (… spiders, robots, scrapers, crawlers, … data mining tools) to 'scrape' or download data". No public API.
- **Zapsplat** (zapsplat.com/website-terms-of-use): prohibits "automated access (bots, scrapers, scripts)"; actively monitors and bans.
- **Soundsnap** (soundsnap.com/tos): prohibits access "using any automated means, including … scripts, bots, scrapers, spiders, offline readers, or any tools intended to extract or download content in bulk".
- **Loopcloud**: no public API; ToS is a standard subscription EULA (explicit anti-scraping clause not located, but content is paid/licensed per-download — integration impossible regardless).

---

## 3. Semantic search state of the art (text→audio embedding models, mid-2026)

### 3.1 The contenders

| Model | Audio tower | Params / ckpt size | Emb dim | Notes |
|---|---|---|---|---|
| **LAION-CLAP** `630k-audioset-fusion-best` (= HF `laion/clap-htsat-fused`) | HTS-AT (Swin-like) + "fusion" for variable length | ~190M total (~80M audio, ~110M RoBERTa text); HF safetensors **614 MB** fp32 | **512** | **The one Freesound uses server-side.** github.com/LAION-AI/CLAP; pip `laion-clap` 1.1.7 |
| LAION-CLAP `music_audioset_epoch_15_esc_90.14` | HTS-AT, no fusion | similar | 512 | Music-tuned variant; *different space* — NOT interchangeable with Freesound's |
| **MS-CLAP 2023** (microsoft/CLAP, `msclap` on PyPI) | HTS-AT-22 + GPT-2-based text enc. | ~160M | **1024** | Strong zero-shot; CLAPCap captioning; different dim/space |
| **M2D-CLAP** (NTT, Interspeech 2024; extended TASLP paper arXiv:2503.22104, Mar 2025) | M2D ViT | ~90M audio | 768 | SOTA-ish general audio features; beats CLAP-2023 on clustering/retrieval benchmarks; checkpoints on GitHub (nttcslab/m2d) |
| **SLAP** (arXiv:2601.12594, **Jan 2026**) | redesigned Transformer, **native variable-duration audio** | n/a (paper-fresh) | n/a | Trained on 109M pairs; SOTA ESC-50 95.5%; directly attacks the fixed-10s limitation. **No public checkpoints confirmed yet** — watch it |
| **AudioMuse-AI DCLAP** (github.com/NeptuneHub/AudioMuse-AI-DCLAP) | distilled student (mn10as/EfficientAT + EdgeNeXt) | **~7M audio tower**; ships **ONNX** | 512, **same space as LAION-CLAP teacher** (music checkpoint) | Distilled audio tower, 5–6× faster, runs on a Raspberry Pi 5; reuses the LAION text tower exported to ONNX. Proof that the ONNX path is viable |
| MAEB — "Massive Audio Embedding Benchmark" (arXiv:2602.16008, **Feb 2026**) | — | — | — | New standard benchmark comparing audio embedders incl. both LAION-CLAP variants; consult it before switching models |

(Param counts marked approximate are inferred from HF file sizes and papers, not all independently verified.)

### 3.2 CPU cost (estimates, flagged)

- **Text encoder only** (RoBERTa-base, ~110M params, one short sentence): **tens of ms** on a modern desktop CPU with ONNX Runtime — interactive, trivially.
- **Audio tower** (HTS-AT, 10-s clip @ 48 kHz, fp32 CPU): community datapoint — AudioMuse reports full music tracks (3–5 min, chunked into 10-s windows) take 30–60 s on Apple M-series / 60–120 s on Intel with PyTorch ⇒ roughly **1–3 s per 10-s clip in PyTorch CPU, ~0.5–1.5 s with ONNX Runtime** on a modern desktop CPU (estimate; benchmark locally). The distilled DCLAP tower is 5–6× faster (~100–300 ms/clip class) at some quality cost.
- Implication: locally embedding a 50k-sample library at ~1 s/clip ≈ 14 CPU-hours — fine as a background job; embedding *search results on the fly* is not needed (Freesound already has the vectors).

### 3.3 Short-sample / loop weakness (verified discussion + inference)

- CLAP-family models are trained on ~10-s, caption-style clips (AudioCaps/Clotho/LAION-630k). Shorter inputs get zero-padded to 10 s; the SLAP paper (Jan 2026) explicitly calls out that "most CLAP models … are trained with short and fixed-duration audio clips … shorter segments are padded … leading to … potential information loss." For a 200 ms kick drum, ~98% of the model's input is silence — embeddings of one-shots cluster poorly and text like "punchy 808 kick" matches mushily. This matches widespread practitioner experience; treat it as real.
- Loops fare better (1–10 s of content), but **musical attributes (BPM, key) are essentially invisible to CLAP** — "140 bpm dark techno loop" will match "dark techno loop" and ignore "140 bpm".
- **Mitigation that fits this project perfectly:** let CLAP do the *semantic* part and let Freesound's **symbolic descriptor filters** do the *musical* part: parse bpm/key/duration/"loop"/"one-shot" out of the query (trivial regex or local LLM) and translate to `filter=bpm:[138 TO 142] tonality:"F minor" loopable:true`, sending only the residual text to the embedding/`query`. This hybrid beats pure-CLAP for a DAW use case and is cheap to build.

---

## 4. Practical architecture: who runs the search?

Three options considered:

1. **Query-time search on Freesound's engine** (lexical `query` + filters + `similar_to` CLAP space) — zero infrastructure, zero index, always-fresh catalog, and since late-2025 it *includes* the CLAP semantic capability. Limit: 60 req/min is far beyond interactive needs.
2. **Local embedding index** of Freesound — would require downloading/embedding hundreds of thousands of previews: bandwidth-abusive (explicit ToS violation), stale, weeks of compute. **Rejected** for the online catalog.
3. **Hybrid (recommended):** Freesound's engine for the online catalog (with locally computed *text* embeddings as `similar_to` vectors for semantic queries), plus a **local CLAP index over the user's own downloaded/imported samples** so the same search box searches "my sounds" and "Freesound" together. Local embedding uses the *same* checkpoint (`laion/clap-htsat-fused`) so one text vector queries both backends.

§9 spells out the recommendation.

---

## 5. Local index tooling (tens of thousands of samples)

- **sqlite-vec** (asg017): current stable **v0.1.9, released 2026-03-31** (bug-fix); v0.1.10 alphas in April 2026. Still **pre-v1 alpha, brute-force only (no ANN — tracking issue #25 open)**, single C file, runs everywhere, metadata columns + KNN in SQL. Benchmarks: 100k × 768-d float vectors ≈ <75 ms per query; 512-d will be comfortably under that. Maintenance had a worrying 6-month gap in 2025 (issue #226) but resumed. For ~10k–100k user samples at 512 dims, **brute force is plenty and sqlite-vec is the lowest-friction choice** — the sample DB (paths, tags, license, source URL, md5) wants to be SQLite anyway, so embeddings live next to metadata in one file.
- **hnswlib** — header-only C++ HNSW, mature, fine, but separate index file + manual persistence/id-mapping.
- **usearch** (unum-cloud) — modern single-header C++ HNSW, faster than FAISS at small scale, SIMD, good C++ API; best pick *if* brute force ever becomes too slow.
- **FAISS** — overkill (build complexity, MKL/BLAS baggage) below millions of vectors. Skip.
- Fallback worth noting: at 50k × 512-d fp32 = 100 MB, even a hand-rolled `memcpy`-into-RAM + SIMD dot-product scan is ~10 ms. Don't over-engineer.
- **Dedup/tagging:** primary dedup key = **Freesound `md5` field** (exact dupes, verified API field) + file-content md5 locally. For near-dupes (re-encodes, trims): **Chromaprint** (acoustid/chromaprint, C lib, GPL-compatible LGPL2.1+) — designed exactly for "duplicate audio file detection", ~2.5 KB fingerprint, <100 ms for 2 min audio; weak on very short (<~3 s) samples (its config needs minimum audio length) — for one-shots, CLAP-embedding cosine similarity (>0.98) is itself a good near-dupe detector. Inference: combine md5 (exact) + chromaprint (loops/recordings) + embedding-distance (one-shots).

---

## 6. Inference integration for a C++ app

Options for running the CLAP **text** encoder (required for the semantic path) and optionally the **audio** tower (for local-library indexing):

1. **ONNX Runtime embedded in-process** — ORT is on a monthly release cadence, **v1.26.0 (May 2026)**, C++ API, MIT, ships fine in a GPLv3 app, CPU EP is a single `.dylib` on macOS (universal2 builds cover both Apple-silicon and Intel; the optional CoreML EP is also available).
   - **Text tower: easy.** RoBERTa-based text encoder exports cleanly (standard transformer). Pre-exported community ONNX exists: **`Xenova/clap-htsat-unfused`** on Hugging Face has ONNX weights (for transformers.js), and AudioMuse-DCLAP ships `clap_text_model.onnx` exported from LAION-CLAP. Tokenizer = RoBERTa-base; in C++ use HuggingFace `tokenizers` (Rust lib with C bindings) or reimplement BPE (small, well-specified).
   - **Audio tower: the painful part (verified pain).** The mel-spectrogram front-end is *outside* the exportable graph (LAION-CLAP uses torchaudio/custom STFT; torch ONNX export of STFT/MFCC is broken/unsupported — pytorch issue #125375). You must reimplement: 48 kHz resample → int16-quantize-roundtrip (yes, really — see DCLAP code; LAION's preprocessing quantizes) → STFT (n_fft 1024/hop 480, model-config-specific) → 64-mel log-mel (HTSAT config: 64 mels, 10-s @ 48 kHz) → fusion logic for >10 s clips (3 crops + global, model-internal). Adobe's **convmelspec** (github.com/adobe-research/convmelspec) bakes the melspec into the ONNX graph as conv layers — the cleanest escape hatch. Any mismatch in preprocessing silently degrades embeddings, and you must validate against Python reference outputs.
2. **Localhost Python sidecar** (FastAPI/uvicorn + torch or onnxruntime + `laion_clap`/`transformers`) — ~50 lines of Python; exposes `POST /embed_text`, `POST /embed_audio`; C++ side is a trivial HTTP+JSON client (JUCE `URL` class). Eliminates the entire preprocessing-reimplementation risk because you run LAION's own reference code. Costs: shipping/embedding a Python runtime (~150–300 MB with torch-cpu; ~60 MB with onnxruntime-only), process lifecycle management. The macOS application firewall only governs *incoming* connections, and binding the sidecar to `127.0.0.1` (loopback) is not treated as an incoming network listener, so it does not trigger a firewall prompt; just note that a notarized/sandboxed app would need the right entitlements to spawn a helper process.
3. **ggml/llama.cpp-style native port** — **no maintained CLAP ggml port exists as of mid-2026** (verified absence-of-evidence: searches surface no clap.cpp). Whisper.cpp/encodec.cpp show it's possible; it's weeks of work. Not for a solo dev.

**Recommendation for a solo dev:** start with the **Python sidecar** for development and for the audio-tower/local-indexing path (reference-correct embeddings, zero export pain), but implement the **text encoder via in-process ONNX Runtime** early — it's the only piece needed for online semantic search, it's the easy export, and it removes the sidecar from the critical user path (search works even if the sidecar is dead; local-library indexing is a background luxury). Long-term, optionally replace the sidecar's audio tower with ONNX (convmelspec) or the pre-distilled DCLAP ONNX models if their space matches needs.

---

## 7. What still needs prototyping (cannot be verified from docs)

1. Does `similar_to=<text-embedding-vector>&similarity_space=laion_clap` return semantically sensible results? (1-day Python spike with `freesound-python` + `laion_clap`. Decides the whole semantic feature.)
2. Interaction of `query` + `similar_to` + `sort` in one request (undocumented precedence).
3. Whether Freesound OAuth2 redirect URLs may be `http://localhost:<port>` (else use the documented paste-the-code fallback).
4. Live-site docs == repo master (check https://freesound.org/docs/api/ renders the similarity-space table).
5. Exact `license` string returned for legacy Sampling+ sounds.
6. Real CPU latency of `laion/clap-htsat-fused` text + audio towers on the dev machine.

---

## 8. Key hurdles

1. **OAuth2 in a desktop app.** Needed only for original-quality downloads. `client_secret` can't be hidden in a GPLv3 binary (acceptable, known industry practice); 24 h token + refresh-token churn means building token storage (store the refresh token in the **macOS Keychain** via the Security framework / `SecItem` APIs) and refresh logic; loopback-redirect support unverified (fallback: Freesound's official copy-paste code page). UX cost: a one-time browser round-trip per user.
2. **Rate limits & key distribution.** 60/min/2,000/day **per API key**. One embedded key shared by all users won't scale past a handful; per-user keys add onboarding friction; higher limits = email MTG. Also: **API ToS is non-commercial only** — if the DAW is ever sold, Freesound API licensing must be negotiated (mtg@upf.edu).
3. **CLAP on short samples.** One-shots (<1 s) embed poorly (zero-pad to 10 s washes them out); BPM/key are invisible to CLAP. Mitigation: hybrid query parsing → symbolic filters (`bpm`, `tonality`, `loopable`, `single_event`, `duration`) that Freesound precomputes, with CLAP handling only the timbral/semantic residue. SLAP (Jan 2026) may fix variable-duration natively — no checkpoints yet.
4. **ONNX export pain (audio tower only).** STFT/mel front-end not exportable; must be reimplemented bit-exactly in C++ (incl. LAION's int16-quantization quirk and fusion crops) or baked in via convmelspec. Avoided entirely by the sidecar, or by the pre-exported DCLAP/Xenova ONNX artifacts. The *text* tower exports cleanly.
5. **Preview audio quality on the timeline.** Previews are lossy (best: ogg ~192 kbps) and may differ in sample rate from the original; fine for audition/sketching, not for release — especially cymbals/tonal material; also clip-trim offsets may shift a few ms vs the original after re-download (mp3 encoder delay). Design a "lossy placeholder → upgrade to original (OAuth2) in place" flow with sample-accurate re-alignment (waveform cross-correlation on replace).
6. **NC-license contamination.** CC-BY-NC results are useless for any released/monetized music; "Sampling+" stragglers and Wikimedia's CC-BY-SA add edge cases. Default the browser to `license:("Attribution" OR "Creative Commons 0")`, surface license as a colored badge, persist license + author + source-URL into clip metadata and the project file, and warn at export if any NC/SA/unknown-licensed asset is in the session.
7. **sqlite-vec immaturity.** Pre-v1, brute-force-only, single maintainer with a 2025 activity gap. Acceptable because the usage is trivial (one virtual table) and escape hatches (usearch/hnswlib, or hand-rolled scan) are cheap; pin the version and wrap vector-store access behind one interface.
8. **Docs/source drift.** Freesound's search stack visibly evolved through Nov 2025 (endpoint replacement, similarity spaces); expect further change. Keep the API client thin and feature-flag the semantic path.

---

## 9. Recommended architecture

**Query-time search on Freesound's own engine, hybridized with a thin local layer. Do not build a local index of Freesound; do build one for the user's own library.**

```
┌─ DAW (C++/JUCE/Tracktion) ─────────────────────────────────────────┐
│ Search box ─► Query parser (bpm/key/dur/license terms → filters)   │
│   ├─ lexical path: GET /apiv2/search/?query=…&filter=…&fields=…    │
│   ├─ semantic path: text ─► CLAP text enc (ONNX Runtime, in-proc)  │
│   │      └► similar_to=[512 floats]&similarity_space=laion_clap    │
│   └─ local path: same text vector ─► sqlite-vec KNN over user lib  │
│ Results list (previews streamed via token auth, waveform PNGs)     │
│ Drag→timeline: import HQ-ogg preview + {author, url, license} tags │
│ "Get original": OAuth2 (browser, refresh-token cached) ─► replace  │
└────────────────────────────────────────────────────────────────────┘
   Background (optional, later): Python sidecar (FastAPI+laion_clap)
   embeds user's local samples → sqlite-vec; chromaprint+md5 dedup.
```

Concretely:
1. **Phase 1 (token auth only):** lexical search + filters + sort + previews + drag-to-timeline with license metadata + attribution export. Fork `MTG/freesound-juce` as the client skeleton. Default filter excludes NC. This alone is the differentiating feature working end-to-end.
2. **Phase 2 (semantic):** ONNX text encoder (`laion/clap-htsat-fused` text tower; tokenizer via HF `tokenizers` C bindings) → `similar_to` vector queries; plus documented `similar_to=<id>` "more like this" on every result. Prototype-validate item §7.1 first.
3. **Phase 3 (local library):** Python sidecar embeds user samples into sqlite-vec inside the existing SQLite sample DB; unified results merging local + Freesound; md5/chromaprint dedup; respect `ai_preference` if anything beyond search is ever done with the data.
4. **Phase 4 (optional sources):** Openverse audio API as a second backend (normalized licenses); Wikimedia/IA via the same abstraction; link-out only for BBC. Never touch Splice/Loopcloud/Zapsplat/Soundsnap.

Why this wins for a solo dev: the hardest parts (corpus-scale CLAP indexing, ANN serving, descriptor extraction) are **already run by Freesound on their servers, in the exact embedding space of a public checkpoint** — the DAW only needs a 110M-param text encoder locally (tens of ms on CPU) and disciplined license plumbing.

---

## 10. Sources

Primary (fetched 2026-06-11):
- Freesound API docs sources, `MTG/freesound` master: `_docs/api/source/{authentication,resources,overview,terms_of_use}.rst` — https://github.com/MTG/freesound/tree/master/_docs/api/source (renders to https://freesound.org/docs/api/)
- Freesound API examples: https://github.com/MTG/freesound/blob/master/apiv2/examples.py
- Freesound API ToS: https://freesound.org/help/tos_api/ (referenced from terms_of_use.rst)
- MTG/freesound-juce: https://github.com/MTG/freesound-juce
- LAION-CLAP: https://github.com/LAION-AI/CLAP ; HF `laion/clap-htsat-fused` (614 MB safetensors); pip `laion-clap` 1.1.7
- AudioMuse-AI-DCLAP README (distilled CLAP, ONNX, Pi-5 perf): https://github.com/NeptuneHub/AudioMuse-AI-DCLAP
- sqlite-vec releases (v0.1.9, 2026-03-31; ANN tracking issue #25; maintenance issue #226): https://github.com/asg017/sqlite-vec
- ONNX Runtime releases (v1.26.0, May 2026): https://github.com/microsoft/onnxruntime/releases
- Chromaprint: https://github.com/acoustid/chromaprint
- convmelspec (Adobe): https://github.com/adobe-research/convmelspec
- torch ONNX STFT/MFCC export failure: https://github.com/pytorch/pytorch/issues/125375

Papers:
- SLAP: Scalable Language-Audio Pretraining (Jan 2026): https://arxiv.org/abs/2601.12594
- MAEB: Massive Audio Embedding Benchmark (Feb 2026): https://arxiv.org/abs/2602.16008
- M2D-CLAP (TASLP extension, Mar 2025): https://arxiv.org/abs/2503.22104
- AI-assisted sound search UX study (CLAP-UI, AES 2025): https://arxiv.org/abs/2504.15575
- MS-CLAP: https://github.com/microsoft/CLAP ; https://pypi.org/project/msclap/

Other sources & ToS:
- Jamendo API: https://developer.jamendo.com/v3.0 ; terms: https://devportal.jamendo.com/api_terms_of_use
- BBC Sound Effects / RemArc licence: https://sound-effects.bbcrewind.co.uk ; coverage: https://www.openculture.com/2024/09/free-download-over-33000-sounds-from-the-bbc-sound-effects-archive.html
- Internet Archive APIs: https://archive.org/services/docs/api/ ; licenseurl filtering: https://archive.org/post/1107525
- Wikimedia Commons API: https://commons.wikimedia.org/wiki/Commons:API
- Openverse audio (aggregates Freesound/Jamendo/Wikimedia): https://api.openverse.org ; https://github.com/WordPress/openverse/issues/1777
- Splice ToS: https://splice.com/terms · Zapsplat: https://www.zapsplat.com/website-terms-of-use/ · Soundsnap: https://www.soundsnap.com/tos
- Sampling+ retirement (CC blog, 2011-09-12): https://creativecommons.org/2011/09/12/celebrating-freesound-2-0-retiring-sampling-licenses/
- DAW-integration precedent: Mixcraft Freesound button: https://acoustica.com/mixcraft-10-manual/freesound-org-button ; Soundly Freesound add-on: https://getsoundly.com/faq/how-can-i-use-the-freesound-library/
- Vector libs: https://github.com/unum-cloud/usearch ; sqlite-vec 100k benchmark: https://1yefuwang1.github.io/vectorlite/markdown/news.html
