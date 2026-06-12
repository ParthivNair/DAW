# src/search

Sound discovery: Freesound API client (forked from `MTG/freesound-juce`, modernized to
`/apiv2/search/`), hybrid query parser (bpm/key/loop terms → symbolic filters, residue → CLAP),
ONNX text encoder, OAuth2 + Keychain token storage, local sample index (SQLite + sqlite-vec),
dedup (md5 / Chromaprint / embedding cosine).

Compliance invariants (see `Research/02` §11 and `Research/03`):
- Default license filter excludes CC-BY-NC; license + author + source URL persist into the
  project file; credits are exportable.
- No catalog mirroring; cache only user-driven content.
- API client stays thin and fixture-testable — no live API calls in CI.
