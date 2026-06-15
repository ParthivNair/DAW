---
name: tracktion-api
description: >-
  Cheat-sheet for the Tracktion Engine APIs THIS repo has actually used and
  verified (headless Engine, programmatic Edit construction, built-in plugins,
  Renderer/RenderTask, TransportControl). Use when wiring engine code in
  src/engine/ or a test and you need the working call sequence rather than
  guessing API shapes. Cites our own files. Points at where the real docs live.
---

# Tracktion Engine — verified usage in EZStudio

Only what is proven in this repo. Don't recall API trivia from memory; if it's not here, read
our files, then the **demo sources** (`libs/tracktion_engine/examples/`), headers, or the JUCE
forum Tracktion category. The Doxygen is thin and the engine is a pinned submodule — never edit it.

`namespace te = tracktion::engine;`

## Engine (headless)

```cpp
te::Engine engine { "EZStudio" };   // simple ctor: default PropertyStorage/UIBehaviour, GUI-free
```

- Tests/renders never open an audio device. Only the GUI session does:
  `engine.getDeviceManager().initialise();` … `closeDevices();`
  (see `src/engine/SineDemoSession.cpp`).
- Code that constructs an `Engine` needs the process-wide `juce::ScopedJuceInitialiser_GUI`
  (Catch2 listener in `tests/TestMain.cpp`) or JUCE singletons report as leaked at exit.

## Edit construction (programmatic) — see `src/engine/SineToneEdit.cpp`

```cpp
auto edit = te::Edit::createSingleTrackEdit (engine, te::Edit::EditRole::forRendering);
edit->getMasterVolumePlugin()->setVolumeDb (0.0f);
auto track = te::getAudioTracks (*edit)[0];
track->getVolumePlugin()->setVolumeDb (0.0f);
```

- **Built-in plugin types**: not all are registered by `PluginManager::initialise()`.
  `ToneGeneratorPlugin` is omitted, so register it first (idempotent):
  `engine.getPluginManager().createBuiltInType<te::ToneGeneratorPlugin>();`
  then `edit->getPluginCache().createNewPlugin (te::ToneGeneratorPlugin::xmlTypeName, {});`
  and `track->pluginList.insertPlugin (plugin, 0, nullptr);`.
- **CachedValue → live parameter**: setting a plugin `CachedValue` (`tone->frequency = …`) does
  NOT refresh the `AutomatableParameter` the DSP reads. After setting CachedValues, loop
  `for (auto p : plugin->getAutomatableParameters()) p->updateFromAttachedValue();`
  (mirrors `restorePluginStateFromValueTree`). Skip it and you get plugin defaults.
- A synth (no clips) has no content length; the caller bounds time via the render `Parameters::time`
  or the transport loop range.

## Renderer / RenderTask — see `tests/render/RenderHelpers.h`

Drive a `Renderer::RenderTask` inline; do NOT call `Renderer::renderToFile(taskDescription, ...)`
headless (it waits on a progress-bar UI that never returns without a message loop).

```cpp
te::Renderer::Parameters params (edit);
params.destFile           = file;
params.audioFormat        = edit.engine.getAudioFileFormatManager().getWavFormat();
params.sampleRateForAudio = sampleRate;
params.blockSizeForAudio  = blockSize;
params.bitDepth           = 24;
params.time               = { tracktion::TimePosition(), tracktion::TimePosition::fromSeconds (len) };
params.usePlugins         = true;
params.tracksToDo         = te::toBitSet (te::getAllTracks (edit));
params.checkNodesForAudio = true;   // fail a silent render instead of writing zeros
te::Renderer::RenderTask task ("render", params, nullptr, nullptr);
while (task.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain) {}
// task.errorMessage is empty on success.
```

## TransportControl — see `src/engine/SineDemoSession.cpp`

```cpp
using namespace tracktion::literals;
auto& transport = edit->getTransport();
transport.setLoopRange ({ 0_tp, tracktion::TimePosition::fromSeconds (durationSecs) });
transport.looping = true;
transport.ensureContextAllocated();   // required before playFromStart on a fresh edit
transport.playFromStart (true);
// ...
transport.stop (false, false);
transport.isPlaying();
```

## Where the real docs are

- **Demo sources**: `libs/tracktion_engine/examples/` — the engine's actual documentation.
- The engine's own `TestRunner` (`TRACKTION_UNIT_TESTS`) shows Edits built in code.
- JUCE forum, Tracktion category. Headers over Doxygen. Hard-won specifics: `dev/decisions.md`
  (Chunk 2 build/PCH layout, Chunk 4 first-sound + render gotchas).
