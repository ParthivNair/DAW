# libs

Third-party code, vendored as **pinned git submodules** (added in Phase 0):

- `tracktion_engine` — pinned `develop` SHA; brings JUCE 8 at `modules/juce`. Build against
  that JUCE only.
- `rubberband` — time-stretch; copied/symlinked into the engine's expected
  `3rd_party/rubberband` path at configure time.
- Test/utility deps (Catch2, melatonin_test_helpers, moodycamel) via submodule or FetchContent.

**Never edit submodule contents.** Local patches live in `/patches` with an apply script and
documentation. Every addition here gets a row in `THIRD_PARTY_LICENSES.md`.
