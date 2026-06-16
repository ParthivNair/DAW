// Phase 1, Chunk 2 — clip edit operations, verified by rendering their audible effect.
//
// Each section imports a known 440 Hz / -9.03 dBFS tone clip onto a fresh arrangement
// at t = 0.4 s, applies one ClipOps gesture, renders, and asserts the audio changed the
// way the gesture should change it:
//   move   -> the clip's audio appears at the new position (silence before it shifts),
//   trim   -> the clip ends earlier (silence appears where the untrimmed tail was),
//   gain   -> the body RMS drops by the requested dB,
//   fadeIn -> the start of the clip is quieter than its steady state,
//   delete -> the clip is gone from the track.
// The source tone WAV is rendered once (static) and re-imported per section.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <melatonin_audio_sparklines/melatonin_audio_sparklines.h>
#include <melatonin_test_helpers/melatonin_test_helpers.h>

#include "engine/ArrangementEdit.h"
#include "engine/ClipImporter.h"
#include "engine/ClipOps.h"
#include "engine/SineToneEdit.h"
#include "render/RenderHelpers.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;
using Catch::Matchers::WithinAbs;

namespace
{
constexpr double sampleRate    = 48000.0;
constexpr double frequencyHz   = 440.0;
constexpr float levelLinear    = 0.5f;
constexpr double srcSecs       = 0.8;    // source clip length
constexpr double clipStartSecs = 0.4;    // where it's placed
constexpr double renderSecs    = 1.5;    // arrangement render window
constexpr float toneRmsDbfs    = -9.03f; // 20*log10(0.5/sqrt(2))

// Renders the known source tone to a temp WAV exactly once for the whole test case.
const juce::File& sourceWavFile()
{
    static std::shared_ptr<juce::TemporaryFile> wav = []
    {
        te::Engine engine { "EZStudio-clipops-source" };
        auto edit = daw::buildSineToneEdit (engine, { frequencyHz, levelLinear, srcSecs },
                                            daw::EditPurpose::offlineRender);
        return daw::render_test::renderEditToWav (*edit, srcSecs, sampleRate);
    }();
    return wav->getFile();
}
} // namespace

TEST_CASE ("Clip operations change the rendered audio as expected", "[render]")
{
    te::Engine engine { "EZStudio-clipops-test" };
    auto edit   = daw::buildArrangementEdit (engine, 1, daw::EditPurpose::offlineRender);
    auto* track = te::getAudioTracks (*edit)[0];
    REQUIRE (track != nullptr);

    auto* clip = daw::importAudioFileAsClip (*track, sourceWavFile(), clipStartSecs);
    REQUIRE (clip != nullptr);

    const auto secs = [] (double s)
    { return (int) std::llround (s * sampleRate); };

    // Renders the current edit and returns the decoded buffer (self-contained: the
    // temp WAV is read fully before it is deleted).
    const auto renderToBuffer = [&]() -> juce::AudioBuffer<float>
    {
        auto wav  = daw::render_test::renderEditToWav (*edit, renderSecs, sampleRate);
        double sr = 0.0;
        auto opt  = daw::render_test::readWavToBuffer (wav->getFile(), sr);
        REQUIRE (opt.has_value());
        return std::move (*opt);
    };

    SECTION ("move shifts the clip's audio to the new position")
    {
        daw::moveClip (*clip, 0.15); // clip now [0.15, 0.95]
        CHECK (clip->getPosition().getStart().inSeconds() == Catch::Approx (0.15).margin (1.0e-4));

        auto buf = renderToBuffer();

        // Tone now present at [0.20, 0.34] (silent if the clip were still at 0.4) ...
        const float atNewPos = daw::render_test::regionRmsDbfs (buf, 0, secs (0.20), secs (0.14));
        // ... while [0, 0.10] (before the moved start) is silent.
        const float before = daw::render_test::regionRmsDbfs (buf, 0, secs (0.0), secs (0.10));
        // The body is still the 440 Hz tone at the source level.
        auto body           = daw::render_test::regionView (buf, secs (0.25), secs (0.6));
        const float bodyRms = daw::render_test::rmsDbfs (body, 0, secs (0.01));
        const auto peak     = daw::render_test::dominantFrequency (body, 0, sampleRate);

        WARN ("[render] move: atNewPos=" << atNewPos << " before=" << before
                                         << " bodyRms=" << bodyRms << " freq=" << peak.frequencyHz);
        CHECK (atNewPos > -20.0f);
        CHECK (before < -90.0f);
        CHECK_THAT (bodyRms, WithinAbs (toneRmsDbfs, 0.5f));
        CHECK_THAT (peak.frequencyHz, WithinAbs (frequencyHz, peak.binResolutionHz));
    }

    SECTION ("trim shortens the clip (tail becomes silent)")
    {
        daw::setClipLength (*clip, 0.4); // clip now [0.4, 0.8]
        CHECK (clip->getPosition().getLength().inSeconds() == Catch::Approx (0.4).margin (1.0e-3));

        auto buf = renderToBuffer();

        const float body = daw::render_test::regionRmsDbfs (buf, 0, secs (0.45), secs (0.30));
        // Where the untrimmed clip (would reach 1.2 s) used to play, now silent.
        const float tail  = daw::render_test::regionRmsDbfs (buf, 0, secs (0.90), secs (0.30));
        const float front = daw::render_test::regionRmsDbfs (buf, 0, secs (0.0), secs (0.30));

        WARN ("[render] trim: body=" << body << " tail=" << tail << " front=" << front);
        CHECK_THAT (body, WithinAbs (toneRmsDbfs, 0.6f));
        CHECK (tail < -90.0f);
        CHECK (front < -90.0f);
    }

    SECTION ("gain drops the body level by the requested dB")
    {
        daw::setClipGainDb (*clip, -6.0f);
        CHECK (clip->getGainDB() == Catch::Approx (-6.0f).margin (1.0e-3));

        auto buf = renderToBuffer();

        const float body = daw::render_test::regionRmsDbfs (buf, 0, secs (0.45), secs (0.30));
        WARN ("[render] gain -6 dB: body=" << body << " (expected " << (toneRmsDbfs - 6.0f) << ")");
        CHECK_THAT (body, WithinAbs (toneRmsDbfs - 6.0f, 0.5f));
    }

    SECTION ("fade-in makes the clip start quieter than its steady state")
    {
        daw::setClipFadeIn (*clip, 0.4); // fade over [0.4, 0.8]
        CHECK (clip->getFadeIn().inSeconds() == Catch::Approx (0.4).margin (1.0e-3));

        auto buf = renderToBuffer();

        const float early  = daw::render_test::regionRmsDbfs (buf, 0, secs (0.42), secs (0.06));
        const float steady = daw::render_test::regionRmsDbfs (buf, 0, secs (0.95), secs (0.20));
        WARN ("[render] fadeIn: early=" << early << " steady=" << steady);
        // The ramp start is well below the steady-state body level.
        CHECK (early < steady - 6.0f);
    }

    SECTION ("delete removes the clip from the track")
    {
        REQUIRE (track->getClips().size() == 1);
        daw::deleteClip (*clip);
        CHECK (track->getClips().size() == 0);
    }
}
