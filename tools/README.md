# tools

Development tooling that is not part of the app:

- `bootstrap.sh` — idempotent fresh-clone initializer: inits the pinned `libs/` submodules
  (with the JUCE SSH→HTTPS fix) and verifies the engine + JUCE pins.
- Golden-fixture regeneration (`daw_regen_goldens` support scripts)
- Spike scripts (e.g. the Phase-B Freesound `similar_to` text-vector prototype, Python)
- The Phase-4 Python embedding sidecar (FastAPI + laion_clap, loopback-only)
- CI helper scripts
