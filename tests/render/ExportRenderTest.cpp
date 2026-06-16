// Phase 1, Chunk 5 — the production export service (daw::exportEditToWav). A two-clip
// arrangement is exported and the resulting WAV is checked:
//   * audio is preserved across sample rates (440 Hz survives 44.1k and 48k, both clips),
//   * the file's sample rate / length / finiteness are right and errorMessage is empty,
//   * the useMasterPlugins toggle decides whether the master fader is applied.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/ArrangementEdit.h"
#include "engine/ArrangementRenderer.h"
#include "engine/ClipImporter.h"
#include "engine/SineToneEdit.h"
#include "render/RenderHelpers.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;
using Catch::Matchers::WithinAbs;

namespace
{
constexpr double frequencyHz = 440.0;
constexpr double srcSecs     = 0.5;
constexpr double clipAStart  = 0.2; // clip A: [0.2, 0.7]
constexpr double clipBStart  = 0.9; // clip B: [0.9, 1.4]
constexpr float toneRmsDbfs  = -9.03f;

const juce::File& exportSourceWavFile()
{
    static std::shared_ptr<juce::TemporaryFile> wav = []
    {
        te::Engine engine { "EZStudio-export-source" };
        auto edit = daw::buildSineToneEdit (engine, { frequencyHz, 0.5f, srcSecs },
                                            daw::EditPurpose::offlineRender);
        return daw::render_test::renderEditToWav (*edit, srcSecs, 48000.0);
    }();
    return wav->getFile();
}

// Builds a fresh two-clip arrangement (both clips the known tone) on one track.
std::unique_ptr<te::Edit> buildTwoClipEdit (te::Engine& engine)
{
    auto edit   = daw::buildArrangementEdit (engine, 1, daw::EditPurpose::offlineRender);
    auto* track = te::getAudioTracks (*edit)[0];
    daw::importAudioFileAsClip (*track, exportSourceWavFile(), clipAStart);
    daw::importAudioFileAsClip (*track, exportSourceWavFile(), clipBStart);
    return edit;
}
} // namespace

TEST_CASE ("exportEditToWav writes a correct WAV and honours its options", "[render]")
{
    te::Engine engine { "EZStudio-export-test" };

    // Exports `edit` with `opts` to a temp WAV and returns the decoded buffer + the rate.
    const auto exportAndRead = [] (te::Edit& edit, daw::ExportOptions opts,
                                   double& sampleRateOut) -> juce::AudioBuffer<float>
    {
        juce::TemporaryFile out (".wav");
        const auto res = daw::exportEditToWav (edit, out.getFile(), opts);
        REQUIRE (res.errorMessage.isEmpty());
        REQUIRE (res.success);
        REQUIRE (out.getFile().existsAsFile());
        auto opt = daw::render_test::readWavToBuffer (out.getFile(), sampleRateOut);
        REQUIRE (opt.has_value());
        return std::move (*opt);
    };

    SECTION ("audio + sample rate are preserved across 48k and 44.1k exports")
    {
        auto edit = buildTwoClipEdit (engine);

        for (const double sr : { 48000.0, 44100.0 })
        {
            daw::ExportOptions opts;
            opts.sampleRate = sr;
            opts.bitDepth   = 24;

            double readSr = 0.0;
            auto buf      = exportAndRead (*edit, opts, readSr);

            const auto secs = [readSr] (double s)
            { return (int) std::llround (s * readSr); };

            CHECK (readSr == Catch::Approx (sr));
            // Whole-edit export runs to clip B's end (~1.4 s).
            CHECK (buf.getNumSamples() / readSr == Catch::Approx (clipBStart + srcSecs).margin (0.05));
            REQUIRE_FALSE (daw::render_test::hasNonFinite (buf));

            // Both clips render the 440 Hz tone.
            auto bodyA       = daw::render_test::regionView (buf, secs (clipAStart + 0.05), secs (0.4));
            auto bodyB       = daw::render_test::regionView (buf, secs (clipBStart + 0.05), secs (0.4));
            const auto peakA = daw::render_test::dominantFrequency (bodyA, 0, readSr);
            const auto peakB = daw::render_test::dominantFrequency (bodyB, 0, readSr);
            WARN ("[render] export @ " << sr << ": clipA=" << peakA.frequencyHz
                                       << " Hz clipB=" << peakB.frequencyHz << " Hz");
            CHECK_THAT (peakA.frequencyHz, WithinAbs (frequencyHz, peakA.binResolutionHz));
            CHECK_THAT (peakB.frequencyHz, WithinAbs (frequencyHz, peakB.binResolutionHz));
        }
    }

    SECTION ("useMasterPlugins decides whether the master fader is applied")
    {
        auto edit = buildTwoClipEdit (engine);
        edit->getMasterVolumePlugin()->setVolumeDb (-6.0f); // a deliberate master cut

        const auto bodyRmsOf = [&] (bool useMaster)
        {
            daw::ExportOptions opts;
            opts.sampleRate       = 48000.0;
            opts.useMasterPlugins = useMaster;
            double readSr         = 0.0;
            auto buf              = exportAndRead (*edit, opts, readSr);
            return daw::render_test::regionRmsDbfs (buf, 0, (int) std::llround ((clipAStart + 0.05) * readSr),
                                                    (int) std::llround (0.4 * readSr));
        };

        const float withMaster    = bodyRmsOf (true);  // master -6 dB applied
        const float withoutMaster = bodyRmsOf (false); // master skipped
        WARN ("[render] master toggle: with=" << withMaster << " without=" << withoutMaster);

        CHECK_THAT (withoutMaster, WithinAbs (toneRmsDbfs, 0.5f));
        CHECK_THAT (withMaster, WithinAbs (toneRmsDbfs - 6.0f, 0.5f));
    }
}
