# src/ui

All hand-built UI: timeline/arrangement view, waveform rendering, clip components, mixer,
transport bar, sound browser, plugin windows, settings. The engine provides zero UI.

Rules of thumb (from `Research/01` and `Research/06`):
- Cache rendered waveform strips as tiled images; draw only visible ranges.
- One top-level `DocumentWindow` per hosted plugin editor.
- Keep a runtime renderer toggle (CoreGraphics / software) for profiling.
- UI mutates the model on the message thread only, inside `UndoManager` transactions, and
  updates itself by listening to ValueTree changes.
