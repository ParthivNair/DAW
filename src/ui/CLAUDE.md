# src/ui — rules

- JUCE GUI shell only. It talks to `daw_core`'s public API (e.g. `daw::SineDemoSession`,
  `daw::engineInfoString()`); it does NOT reimplement engine logic. Engine work belongs in
  `src/engine/`.
- This target carries the `EZStudioPCH.h` PCH (juce_gui_extra + tracktion umbrella). Don't add
  ad-hoc heavy includes that the PCH already provides.
- Keep tracktion/engine types out of UI *headers* where avoidable (forward-declare, pass through
  the `daw_core` API) so the GUI/engine boundary stays thin and compile times stay low.
- UI mutates the model on the message thread only; never call into the audio path directly —
  cross-thread traffic goes through `src/rt/`.
