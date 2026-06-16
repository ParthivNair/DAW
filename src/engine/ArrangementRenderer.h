#pragma once

#include <juce_core/juce_core.h>

namespace tracktion
{
inline namespace engine
{
    class Edit;
} // namespace engine
} // namespace tracktion

namespace daw
{
/** Options for exporting an Edit to a WAV file. */
struct ExportOptions
{
    double sampleRate     = 44100.0; ///< Output sample rate.
    int bitDepth          = 24;      ///< WAV bit depth (16/24/32).
    int blockSize         = 512;     ///< Render block size.
    bool useMasterPlugins = true;    ///< Include the master fader/FX in the export.
    bool failIfSilent     = true;    ///< Fail (don't write zeros) if the edit makes no audio.

    /** Render range, in seconds. If `endSecs < 0`, the whole edit (0 .. edit length) is
        exported. */
    double startSecs = 0.0;
    double endSecs   = -1.0;
};

/** Result of an export. */
struct ExportResult
{
    bool success = false;
    juce::String errorMessage; ///< Empty on success; the engine's message on failure.
    double lengthSeconds = 0.0;
};

/** Renders an Edit to a WAV file — the GUI-free service behind the timeline's "Export"
    command. Drives a Renderer::RenderTask inline (never Renderer::renderToFile, which
    hangs headless) and pumps the message loop a slice between blocks so a wave clip's
    async file reads don't starve (the same technique tests/render/RenderHelpers.h uses;
    see the Phase 1 Chunk 1 decisions entry).

    All tracks are rendered (tracksToDo = every track). The destination's parent directory
    is created if needed. Returns success + the engine's error message on failure. */
ExportResult exportEditToWav (tracktion::engine::Edit&, const juce::File& destination,
                              const ExportOptions& = {});
} // namespace daw
