// Phase 1, Chunk 4 — project lifecycle round-trip: build an arrangement, save it to a
// .tracktionedit, reload it in a fresh session, and prove the reloaded edit renders
// sample-identically. Also checks dirty-state tracking and the recent-files list.
//
//   newProject + import clip + gain edit  -> render bufA   (dirty == true)
//   save                                  -> dirty == false, file on disk, in recents
//   openProject (fresh session)           -> tracks/clips preserved, dirty == false
//   render bufB                           -> peak diff vs bufA < -96 dBFS (perfect null)

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "engine/ArrangementEdit.h"
#include "engine/ClipImporter.h"
#include "engine/ClipOps.h"
#include "engine/ProjectSession.h"
#include "engine/SineToneEdit.h"
#include "render/RenderHelpers.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace
{
constexpr double sampleRate    = 48000.0;
constexpr double srcSecs       = 0.8;
constexpr double clipStartSecs = 0.4;
constexpr double renderSecs    = 1.4;

juce::AudioBuffer<float> renderEdit (te::Edit& edit)
{
    auto wav  = daw::render_test::renderEditToWav (edit, renderSecs, sampleRate);
    double sr = 0.0;
    auto opt  = daw::render_test::readWavToBuffer (wav->getFile(), sr);
    REQUIRE (opt.has_value());
    return std::move (*opt);
}
} // namespace

TEST_CASE ("A project round-trips through .tracktionedit with a sample-identical render", "[render]")
{
    te::Engine engine { "EZStudio-project-test" };

    // Source tone WAV, held for the whole test: the reloaded edit's clip reads it from
    // disk at render time, so it must outlive both renders.
    auto sourceEdit = daw::buildSineToneEdit (engine, { 440.0, 0.5f, srcSecs },
                                              daw::EditPurpose::offlineRender);
    auto sourceWav  = daw::render_test::renderEditToWav (*sourceEdit, srcSecs, sampleRate);
    REQUIRE (sourceWav->getFile().existsAsFile());

    // The project file lives in the temp dir alongside the source WAV.
    juce::TemporaryFile editTemp (".tracktionedit");
    const auto editFile = editTemp.getFile();

    juce::AudioBuffer<float> bufA;

    SECTION ("build, edit, save, reload, render-null")
    {
        // --- session 1: build + save ---
        {
            daw::ProjectSession session (engine);
            session.newProject (editFile, 2);
            REQUIRE (session.edit() != nullptr);
            REQUIRE (session.hasUnsavedChanges()); // a brand-new project is dirty

            REQUIRE (te::getAudioTracks (*session.edit()).size() == 2);
            auto* track = te::getAudioTracks (*session.edit())[0];
            auto* clip  = daw::importAudioFileAsClip (*track, sourceWav->getFile(), clipStartSecs);
            REQUIRE (clip != nullptr);
            daw::setClipGainDb (*clip, -6.0f); // a non-trivial edit that must survive the round-trip
            REQUIRE (session.hasUnsavedChanges());

            bufA = renderEdit (*session.edit());

            REQUIRE (session.save());
            CHECK_FALSE (session.hasUnsavedChanges()); // clean immediately after save
            CHECK (editFile.existsAsFile());
            CHECK (session.recentFiles().size() >= 1);
        } // session 1 (and its Edit) destroyed here

        // --- session 2: reload ---
        daw::ProjectSession session2 (engine);
        REQUIRE (session2.openProject (editFile));
        CHECK_FALSE (session2.hasUnsavedChanges());
        REQUIRE (session2.edit() != nullptr);

        // Structure preserved.
        REQUIRE (te::getAudioTracks (*session2.edit()).size() == 2);
        auto* track2 = te::getAudioTracks (*session2.edit())[0];
        REQUIRE (track2->getClips().size() == 1);
        CHECK (track2->getClips()[0]->getPosition().getStart().inSeconds() == Catch::Approx (clipStartSecs).margin (1.0e-3));

        // Audio preserved: the reloaded edit renders identically (clip position + gain).
        auto bufB = renderEdit (*session2.edit());
        REQUIRE (bufA.getNumSamples() > 0);

        const float nullDb = daw::render_test::peakDiffDbfs (bufA, bufB);
        WARN ("[render] project round-trip peak diff = " << nullDb << " dBFS (expect < -96)");
        CHECK (nullDb < -96.0f);
        REQUIRE_FALSE (daw::render_test::hasNonFinite (bufB));
    }

    SECTION ("openProject fails cleanly on a missing file")
    {
        daw::ProjectSession session (engine);
        CHECK_FALSE (session.openProject (juce::File ("/no/such/project.tracktionedit")));
        CHECK (session.edit() == nullptr);
    }
}
