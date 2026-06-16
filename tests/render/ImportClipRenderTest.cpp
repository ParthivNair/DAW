// Phase 1, Chunk 1 — import an audio file as a clip and prove it renders correctly.
//
// Strategy: we need a known source signal on disk. Rather than ship a fixture, we
// RENDER one: buildSineToneEdit (440 Hz, level 0.5) -> a temp WAV. That file is a
// pure -9.03 dBFS RMS / 440 Hz sine. We then import it onto an arrangement track at
// t = 2.0 s and render the whole arrangement, asserting:
//   (a) the region before the clip (t < 2.0 s) is silent,
//   (b) the clip body is a 440 Hz sine at the source's -9.03 dBFS RMS,
//   (c) the output is finite and non-silent overall.
// This exercises the real import path (te::AudioFile validation + insertWaveClip with
// an explicit ClipPosition) end-to-end through a render — the engine reads the clip's
// source from disk, so the temp WAV must outlive the arrangement render (it does: we
// hold the TemporaryFile for the whole test).

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <melatonin_audio_sparklines/melatonin_audio_sparklines.h>
#include <melatonin_test_helpers/melatonin_test_helpers.h>

#include "engine/ArrangementEdit.h"
#include "engine/ClipImporter.h"
#include "engine/SineToneEdit.h"
#include "render/RenderHelpers.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;
using Catch::Matchers::WithinAbs;

TEST_CASE ("An imported WAV clip renders at its placed position with the source signal", "[render]")
{
    constexpr double sampleRate   = 48000.0;
    constexpr double frequencyHz  = 440.0;
    constexpr float levelLinear   = 0.5f;
    constexpr double sourceSecs   = 1.5;
    constexpr double clipStartSec = 2.0;
    constexpr double totalSecs    = clipStartSec + sourceSecs; // 3.5 s arrangement

    constexpr float expectedRmsDbfs = -9.03f; // 20*log10(0.5/sqrt(2)), same as the source tone
    constexpr float rmsToleranceDb  = 0.5f;

    te::Engine engine { "EZStudio-import-test" };

    // --- 1. Produce a known source WAV by rendering the sine tone edit. ---
    auto sourceEdit = daw::buildSineToneEdit (engine, { frequencyHz, levelLinear, sourceSecs },
                                              daw::EditPurpose::offlineRender);
    REQUIRE (sourceEdit != nullptr);
    auto sourceWav = daw::render_test::renderEditToWav (*sourceEdit, sourceSecs, sampleRate);
    REQUIRE (sourceWav->getFile().existsAsFile());
    REQUIRE (sourceWav->getFile().getSize() > 0);

    // --- 2. Build a 4-track arrangement and import the WAV at t = 2.0 s on track 0. ---
    auto edit = daw::buildArrangementEdit (engine, 4, daw::EditPurpose::offlineRender);
    REQUIRE (edit != nullptr);
    REQUIRE (te::getAudioTracks (*edit).size() == 4);

    auto* track = te::getAudioTracks (*edit)[0];
    REQUIRE (track != nullptr);

    auto* clip = daw::importAudioFileAsClip (*track, sourceWav->getFile(), clipStartSec);
    REQUIRE (clip != nullptr);
    CHECK (clip->getPosition().getStart().inSeconds() == Catch::Approx (clipStartSec).margin (1.0e-4));
    CHECK (clip->getPosition().getLength().inSeconds() == Catch::Approx (sourceSecs).margin (1.0e-3));

    // A bad/missing file must fail loudly (nullptr), not insert a silent clip.
    CHECK (daw::importAudioFileAsClip (*track, juce::File ("/no/such/file.wav"), 0.0) == nullptr);

    // --- 3. Render the whole arrangement. ---
    auto wav = daw::render_test::renderEditToWav (*edit, totalSecs, sampleRate);
    REQUIRE (wav->getFile().existsAsFile());

    double readSampleRate = 0.0;
    auto bufferOpt        = daw::render_test::readWavToBuffer (wav->getFile(), readSampleRate);
    REQUIRE (bufferOpt.has_value());
    auto& buffer = *bufferOpt;

    CHECK (readSampleRate == Catch::Approx (sampleRate));
    REQUIRE (buffer.getNumSamples() > 0);

    // Overall: finite (no NaN/Inf). We deliberately do NOT assert isFilled() on the
    // whole buffer — by design the region before the clip is silent, so only the clip
    // body is "filled"; isFilled() is asserted on the body view below.
    juce::dsp::AudioBlock<float> block (buffer);
    REQUIRE_THAT (block, melatonin::isValidAudio());
    REQUIRE_FALSE (daw::render_test::hasNonFinite (buffer));

    const auto secsToSamples = [readSampleRate] (double s)
    { return (int) std::llround (s * readSampleRate); };

    // (a) Silence before the clip: measure [0.05 s, 1.90 s), well clear of the 2.0 s edge.
    const float preRms = daw::render_test::regionRmsDbfs (buffer, 0, secsToSamples (0.05),
                                                          secsToSamples (1.85));

    // (b)/(c) Clip body: a centred window inside [2.0 s, 3.5 s], 50 ms clear of each edge.
    auto body           = daw::render_test::regionView (buffer, secsToSamples (clipStartSec + 0.05),
                                                        secsToSamples (sourceSecs - 0.1));
    const float bodyRms = daw::render_test::rmsDbfs (body, 0, secsToSamples (0.005));
    const auto peak     = daw::render_test::dominantFrequency (body, 0, readSampleRate);

    // The clip body must actually carry audio (not silence).
    juce::dsp::AudioBlock<float> bodyBlock (body);
    REQUIRE_THAT (bodyBlock, melatonin::isFilled());

    {
        juce::AudioBuffer<float> head (body.getArrayOfWritePointers(), 1,
                                       juce::jmin (body.getNumSamples(), 220));
        WARN ("[render] imported-clip body sparkline (first ~2 cycles): "
              << melatonin::sparkline (head).toStdString());
        WARN ("[render] pre-clip region RMS = " << preRms << " dBFS (expect << -90)");
        WARN ("[render] clip body RMS = " << bodyRms << " dBFS (expected " << expectedRmsDbfs
                                          << " +/- " << rmsToleranceDb << ")");
        WARN ("[render] clip body dominant bin " << peak.binIndex << " -> " << peak.frequencyHz
                                                 << " Hz (expected " << frequencyHz << ", +/- "
                                                 << peak.binResolutionHz << " Hz)");
    }

    // (a) The pre-clip region is genuine silence (exact zeros render to the dB floor).
    CHECK (preRms < -90.0f);
    // (b) The clip body carries the source level ...
    CHECK_THAT (bodyRms, WithinAbs (expectedRmsDbfs, rmsToleranceDb));
    // ... and the source frequency, proving the right samples play at the right place.
    CHECK_THAT (peak.frequencyHz, WithinAbs (frequencyHz, peak.binResolutionHz));
}
