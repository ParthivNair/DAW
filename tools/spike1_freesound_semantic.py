#!/usr/bin/env python3
"""Spike 1 — Freesound semantic-search validation (dev/TODO.md Part B).

Tests the undocumented trick the Phase-4 headline feature depends on: embedding a
*text* query locally with LAION-CLAP (checkpoint `630k-audioset-fusion-best`, the one
Freesound runs server-side) and passing the 512-float vector as
`similar_to=[...]&similarity_space=laion_clap` to `GET /apiv2/search/`.

Also probes the adjacent unknowns from Research/03 §7: `query`+`similar_to` precedence,
license-filter composition, `/sounds/<id>/analysis/` vector retrieval, and the license
string returned for legacy (pre-2011 Sampling+) sounds.

Usage (venv: ~/venvs/daw):
    python tools/spike1_freesound_semantic.py embed     # local: model -> cached vectors
    python tools/spike1_freesound_semantic.py search    # live: semantic vs lexical eval
    python tools/spike1_freesound_semantic.py probes    # live: undocumented-behavior probes
    python tools/spike1_freesound_semantic.py all

`embed` needs torch + laion_clap (slow import, ~2.3 GB checkpoint download on first run).
`search`/`probes` need only `requests` plus an API key from https://freesound.org/apiv2/apply,
supplied via $FREESOUND_API_KEY or the git-ignored file tools/.freesound_key.

Outputs land in tools/spike1_out/ (git-ignored): text_vectors.json + per-step raw JSON +
a human-readable transcript on stdout. Verdict gets recorded manually in dev/decisions.md.
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

API_BASE = "https://freesound.org/apiv2"
HERE = Path(__file__).resolve().parent
OUT_DIR = HERE / "spike1_out"
VECTORS_FILE = OUT_DIR / "text_vectors.json"

RESULT_FIELDS = "id,name,tags,username,license,duration,type,samplerate,score"

# ~10 varied queries: timbral, scenic, percussive one-shot (CLAP's weak spot),
# and one with BPM wording (CLAP is expected to be blind to it — hybrid parser motivation).
QUERIES = [
    "vinyl crackle loop",
    "punchy kick drum one shot",
    "rain falling on a tent",
    "warm analog synth pad",
    "glass breaking",
    "dark ambient drone",
    "birds singing in a forest at dawn",
    "deep 808 sub bass",
    "footsteps on gravel",
    "cinematic riser whoosh",
    "dusty vinyl crackle loop 90 bpm",
]


# ---------------------------------------------------------------- local half


def cmd_embed() -> None:
    """Embed QUERIES with laion_clap 630k-audioset-fusion-best; cache to JSON."""
    import numpy as np
    import torch

    # torch >= 2.6 defaults torch.load to weights_only=True, which laion_clap's
    # load_ckpt doesn't pass; the LAION checkpoint needs full unpickling (trusted source).
    _orig_load = torch.load

    def _load(*args, **kwargs):
        kwargs.setdefault("weights_only", False)
        return _orig_load(*args, **kwargs)

    torch.load = _load

    import laion_clap

    print("Loading CLAP_Module(enable_fusion=True), checkpoint 630k-audioset-fusion-best ...")
    model = laion_clap.CLAP_Module(enable_fusion=True)
    model.load_ckpt(model_id=3)  # 3 = 630k-audioset-fusion-best.pt — must match Freesound

    t0 = time.time()
    emb = model.get_text_embedding(QUERIES, use_tensor=False)
    dt = time.time() - t0
    emb = np.asarray(emb, dtype=np.float64)
    print(f"Embedded {len(QUERIES)} queries in {dt:.2f}s -> shape {emb.shape}")
    assert emb.shape == (len(QUERIES), 512), f"expected (n, 512), got {emb.shape}"

    norms = np.linalg.norm(emb, axis=1)
    print(f"Norms before explicit normalization: min={norms.min():.6f} max={norms.max():.6f}")
    emb = emb / norms[:, None]  # API L2-normalizes anyway; do it ourselves for cleanliness

    OUT_DIR.mkdir(exist_ok=True)
    VECTORS_FILE.write_text(
        json.dumps({q: v.tolist() for q, v in zip(QUERIES, emb)}, indent=1)
    )
    print(f"Wrote {VECTORS_FILE}")


# ----------------------------------------------------------------- live half


def api_key() -> str:
    import os

    key = os.environ.get("FREESOUND_API_KEY", "").strip()
    keyfile = HERE / ".freesound_key"
    if not key and keyfile.exists():
        key = keyfile.read_text().strip()
    if not key:
        sys.exit(
            "No API key. Create one at https://freesound.org/apiv2/apply then either\n"
            "  export FREESOUND_API_KEY=...   or   echo KEY > tools/.freesound_key"
        )
    return key


def get(path: str, label: str, **params) -> dict:
    """GET with token auth, 1.1s throttle (60/min limit), one 429 retry, raw-JSON dump.

    The query string is built by hand: the server (gunicorn) caps the request line at
    4094 chars, and requests' params= percent-encodes every comma in the 512-float
    vector (x3 size). Commas/brackets are legal raw in query strings — keep them raw.
    """
    import requests
    from urllib.parse import quote

    time.sleep(1.1)
    qs = "&".join(f"{k}={quote(str(v), safe='[],')}" for k, v in params.items())
    url = f"{API_BASE}{path}" + (f"?{qs}" if qs else "")
    if len(url) > 4000:
        print(f"  WARNING: url length {len(url)} likely exceeds the 4094 request-line cap")
    resp = requests.get(url, headers={"Authorization": f"Token {api_key()}"})
    if resp.status_code == 429:
        print("  ... 429 throttled, sleeping 65s")
        time.sleep(65)
        resp = requests.get(url, headers={"Authorization": f"Token {api_key()}"})
    print(f"  [{label}] HTTP {resp.status_code}  url-length={len(resp.request.url)}")
    try:
        data = resp.json()
    except ValueError:
        data = {"_non_json_body": resp.text[:2000]}
    OUT_DIR.mkdir(exist_ok=True)
    (OUT_DIR / f"{label}.json").write_text(json.dumps(data, indent=1))
    if resp.status_code != 200:
        print(f"  !! error body: {json.dumps(data)[:500]}")
    return data


def vec_param(vector: list[float]) -> str:
    """similar_to as a bracketed float list, squeezed under the 4094-char request-line cap.

    3 decimals: quantization noise on an L2-normed 512-d vector keeps cosine(original,
    rounded) > 0.9998 — irrelevant for retrieval, and the server re-normalizes anyway.
    """

    def fmt(x: float) -> str:
        s = f"{x:.3f}".rstrip("0").rstrip(".")
        return "0" if s in ("", "-0", "-") else s

    return "[" + ",".join(fmt(x) for x in vector) + "]"


def load_vectors() -> dict[str, list[float]]:
    if not VECTORS_FILE.exists():
        sys.exit(f"{VECTORS_FILE} missing — run the `embed` step first")
    return json.loads(VECTORS_FILE.read_text())


def show(data: dict, n: int = 8) -> list[int]:
    """Print top-n results compactly; return their ids."""
    results = data.get("results", [])
    ids = []
    for r in results[:n]:
        ids.append(r.get("id"))
        tags = ",".join((r.get("tags") or [])[:6])
        print(
            f"    {r.get('id'):>7}  {r.get('name', '')[:46]:<46} "
            f"{(r.get('license') or '')[:28]:<28} {r.get('duration', 0):6.1f}s  [{tags[:60]}]"
        )
    print(f"    (count={data.get('count')})")
    return ids


def cmd_search() -> None:
    """Headline test: semantic (text vector similar_to) vs lexical, per query."""
    vectors = load_vectors()
    for i, (query, vec) in enumerate(vectors.items()):
        print(f"\n=== Q{i:02d}: {query!r}")
        print("  -- semantic: similar_to=<text vector>, similarity_space=laion_clap")
        sem = get(
            "/search/",
            f"q{i:02d}_semantic",
            similar_to=vec_param(vec),
            similarity_space="laion_clap",
            fields=RESULT_FIELDS,
            page_size=10,
        )
        show(sem)
        print("  -- lexical: query=<text> (baseline for comparison)")
        lex = get("/search/", f"q{i:02d}_lexical", query=query, fields=RESULT_FIELDS, page_size=10)
        show(lex)


def overlap(a: list[int], b: list[int]) -> str:
    inter = len(set(a) & set(b))
    return f"{inter}/{max(len(a), 1)} ids shared"


def cmd_probes() -> None:
    vectors = load_vectors()
    vinyl = vectors["vinyl crackle loop"]

    print("\n=== P1: query + similar_to interaction (undocumented precedence)")
    a = show(get("/search/", "p1_simonly", similar_to=vec_param(vinyl),
                 similarity_space="laion_clap", fields=RESULT_FIELDS, page_size=10))
    b = show(get("/search/", "p1_queryonly", query="piano", fields=RESULT_FIELDS, page_size=10))
    c = show(get("/search/", "p1_both", query="piano", similar_to=vec_param(vinyl),
                 similarity_space="laion_clap", fields=RESULT_FIELDS, page_size=10))
    print(f"  both-vs-semantic-only: {overlap(c, a)};  both-vs-query-only: {overlap(c, b)}")

    print("\n=== P2: license + duration filter composed with semantic similar_to")
    d = get("/search/", "p2_filtered", similar_to=vec_param(vinyl),
            similarity_space="laion_clap",
            filter='license:("Attribution" OR "Creative Commons 0") duration:[* TO 10]',
            fields=RESULT_FIELDS, page_size=10)
    ids = show(d)
    bad = [r for r in d.get("results", [])
           if r.get("license") and not any(s in r["license"] for s in ("by", "zero", "publicdomain"))
           and "Attribution" not in r["license"] and "Creative Commons 0" not in r["license"]]
    print(f"  filtered-vs-unfiltered ordering: {overlap(ids, a)}; non-matching licenses: {len(bad)}")

    print("\n=== P3: /sounds/<id>/analysis/ — can we pull Freesound's stored vectors?")
    target = (a or [1234])[0]
    analysis = get(f"/sounds/{target}/analysis/", "p3_analysis")
    arrays = find_512(analysis)
    print(f"  keys: {list(analysis)[:20]}")
    print(f"  512-float arrays found at: {arrays or 'NONE'}")
    if arrays:
        import math
        flat = analysis
        for part in arrays[0].split("."):
            flat = flat[int(part)] if part.isdigit() else flat[part]
        dot = sum(x * y for x, y in zip(flat, vinyl))
        na = math.sqrt(sum(x * x for x in flat))
        print(f"  cosine(text-vector, sound {target} stored vector) = {dot / na:.4f}")

    print("\n=== P4: license strings on legacy (pre-2011) sounds — Sampling+ hunt")
    e = get("/search/", "p4_oldids", filter="id:[2 TO 4000]",
            fields="id,name,license,created", page_size=150, sort="created_asc")
    lic = {}
    for r in e.get("results", []):
        lic[r.get("license")] = lic.get(r.get("license"), 0) + 1
    print(f"  distinct license values on {len(e.get('results', []))} oldest sounds: {lic}")
    f = get("/search/", "p4_sampling_filter", filter='license:"Sampling+"',
            fields="id,name,license", page_size=10)
    print(f"  filter=license:\"Sampling+\" -> count={f.get('count')}, "
          f"detail={f.get('detail', 'n/a')}")
    show(f, 3)

    print("\n=== P5: similarity_space=freesound_classic rejects a 512-d vector? (sanity)")
    g = get("/search/", "p5_wrongspace", similar_to=vec_param(vinyl),
            similarity_space="freesound_classic", fields="id,name", page_size=3)
    print(f"  -> count={g.get('count')}, detail={g.get('detail', 'n/a')}")


def find_512(node, path="") -> list[str]:
    """Recursively locate 512-element numeric lists in a JSON blob."""
    hits = []
    if isinstance(node, list):
        if len(node) == 512 and all(isinstance(x, (int, float)) for x in node[:8]):
            hits.append(path)
        else:
            for i, v in enumerate(node[:50]):
                hits += find_512(v, f"{path}.{i}" if path else str(i))
    elif isinstance(node, dict):
        for k, v in node.items():
            hits += find_512(v, f"{path}.{k}" if path else str(k))
    return hits


# --------------------------------------------------------------------- main

STEPS = {"embed": cmd_embed, "search": cmd_search, "probes": cmd_probes}

if __name__ == "__main__":
    args = sys.argv[1:] or ["all"]
    names = list(STEPS) if args == ["all"] else args
    unknown = [n for n in names if n not in STEPS]
    if unknown:
        sys.exit(f"unknown step(s) {unknown}; choose from {list(STEPS)} or 'all'")
    for name in names:
        STEPS[name]()
