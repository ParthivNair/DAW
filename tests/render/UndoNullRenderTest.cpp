// Phase 1, Chunk 3 — the undo/redo null test, the strongest regression guard on the
// edit model: a gesture followed by undo must restore a *sample-identical* render.
//
//   render baseline (bufA)
//   apply move + gain gestures   -> render bufC (must differ from bufA)
//   undo, undo                   -> render bufB (must equal bufA: peak diff < -96 dBFS)
//   redo, redo                   -> render bufD (must equal bufC)
//
// This proves (a) ClipOps mutations are recorded into the UndoManager as one
// transaction per gesture, and (b) undo/redo round-trips losslessly through a render.

#include <catch2/catch_test_macros.hpp>

#include "engine/ArrangementEdit.h"
#include "engine/ClipImporter.h"
#include "engine/ClipOps.h"
#include "engine/EditUndo.h"
#include "engine/SineToneEdit.h"
#include "render/RenderHelpers.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace
{
constexpr double sampleRate    = 48000.0;
constexpr double srcSecs       = 0.8;
constexpr double clipStartSecs = 0.4;
constexpr double renderSecs    = 1.5;

const juce::File& undoSourceWavFile()
{
    static std::shared_ptr<juce::TemporaryFile> wav = []
    {
        te::Engine engine { "EZStudio-undo-source" };
        auto edit = daw::buildSineToneEdit (engine, { 440.0, 0.5f, srcSecs },
                                            daw::EditPurpose::offlineRender);
        return daw::render_test::renderEditToWav (*edit, srcSecs, sampleRate);
    }();
    return wav->getFile();
}
} // namespace

TEST_CASE ("Undo restores a sample-identical render; redo re-applies it", "[render]")
{
    te::Engine engine { "EZStudio-undo-test" };
    auto edit   = daw::buildArrangementEdit (engine, 1, daw::EditPurpose::offlineRender);
    auto* track = te::getAudioTracks (*edit)[0];
    REQUIRE (track != nullptr);

    auto* clip = daw::importAudioFileAsClip (*track, undoSourceWavFile(), clipStartSecs);
    REQUIRE (clip != nullptr);

    // Attach the UndoManager (headless) and drop the import from the undo history so we
    // only undo the gestures below.
    daw::ensureUndoManagerReady (*edit);
    daw::clearUndoHistory (*edit);
    REQUIRE_FALSE (daw::canUndo (*edit));

    const auto renderToBuffer = [&]() -> juce::AudioBuffer<float>
    {
        auto wav  = daw::render_test::renderEditToWav (*edit, renderSecs, sampleRate);
        double sr = 0.0;
        auto opt  = daw::render_test::readWavToBuffer (wav->getFile(), sr);
        REQUIRE (opt.has_value());
        return std::move (*opt);
    };

    // Baseline.
    const auto bufA = renderToBuffer();

    // Two gestures, each its own transaction.
    daw::moveClip (*clip, 0.15);
    daw::setClipGainDb (*clip, -6.0f);
    REQUIRE (daw::canUndo (*edit));

    const auto bufC = renderToBuffer(); // the changed arrangement

    // The gestures must actually have changed the audio (otherwise the null below is vacuous).
    const float changedDiff = daw::render_test::peakDiffDbfs (bufA, bufC);
    WARN ("[render] baseline-vs-changed peak diff = " << changedDiff << " dBFS (expect loud)");
    CHECK (changedDiff > -20.0f);

    // Undo both gestures -> back to baseline.
    daw::undo (*edit); // undo gain
    daw::undo (*edit); // undo move
    CHECK_FALSE (daw::canUndo (*edit));

    const auto bufB    = renderToBuffer();
    const float nullDb = daw::render_test::peakDiffDbfs (bufA, bufB);
    WARN ("[render] undo null: baseline-vs-undone peak diff = " << nullDb << " dBFS (expect < -96)");
    CHECK (nullDb < -96.0f);

    // Redo both gestures -> back to the changed render.
    daw::redo (*edit); // redo move
    daw::redo (*edit); // redo gain
    CHECK (daw::canRedo (*edit) == false);

    const auto bufD     = renderToBuffer();
    const float redoNul = daw::render_test::peakDiffDbfs (bufC, bufD);
    WARN ("[render] redo null: changed-vs-redone peak diff = " << redoNul << " dBFS (expect < -96)");
    CHECK (redoNul < -96.0f);

    REQUIRE_FALSE (daw::render_test::hasNonFinite (bufB));
}
