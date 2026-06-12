// Phase 0 prototype render test — the template every future render test copies.
//
// Pipeline:  buildSineToneEdit (440 Hz sine, level 0.5)
//         ->  renderEditToWav   (headless, blocking, synchronous)
//         ->  readWavToBuffer   (juce::AudioFormatReader)
//         ->  assert numeric:    RMS dBFS, dominant FFT bin, finite + non-silent.
//
// Signal: a ToneGeneratorPlugin sine of linear amplitude 0.5. A full-scale sine
// (amplitude 1.0) is 0 dBFS peak / -3.01 dBFS RMS, so amplitude 0.5 is:
//     peak = 20*log10(0.5)        = -6.02 dBFS
//     RMS  = 20*log10(0.5/sqrt2)  = -9.03 dBFS
// We assert the RMS with a +/-0.5 dB tolerance (covers dither/edge-window slop)
// and the dominant frequency within one FFT bin of 440 Hz.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <melatonin_audio_sparklines/melatonin_audio_sparklines.h>
#include <melatonin_test_helpers/melatonin_test_helpers.h>

#include "engine/SineToneEdit.h"
#include "render/RenderHelpers.h"

namespace te = tracktion::engine;
using Catch::Matchers::WithinAbs;

TEST_CASE ("440 Hz sine renders to a WAV with the expected RMS and peak frequency", "[render]")
{
    constexpr double sampleRate  = 48000.0;
    constexpr double frequencyHz = 440.0;
    constexpr float  levelLinear = 0.5f;
    constexpr double lengthSecs  = 1.5;

    // Expected steady-state RMS of a sine of amplitude 0.5: 0.5 / sqrt(2).
    constexpr float expectedRmsDbfs = -9.03f;   // 20*log10(0.5/sqrt(2))
    constexpr float rmsToleranceDb  = 0.5f;

    // Headless engine: no audio device is ever opened in the render path.
    te::Engine engine { "EZStudio-render-test" };

    auto edit = daw::buildSineToneEdit (engine, { frequencyHz, levelLinear, lengthSecs });
    REQUIRE (edit != nullptr);

    auto wav = daw::render_test::renderEditToWav (*edit, lengthSecs, sampleRate);
    REQUIRE (wav->getFile().existsAsFile());
    REQUIRE (wav->getFile().getSize() > 0);

    double readSampleRate = 0.0;
    auto bufferOpt = daw::render_test::readWavToBuffer (wav->getFile(), readSampleRate);
    REQUIRE (bufferOpt.has_value());
    auto& buffer = *bufferOpt;

    CHECK (readSampleRate == Catch::Approx (sampleRate));
    REQUIRE (buffer.getNumSamples() > 0);

    // (c) finite + non-silent.
    juce::dsp::AudioBlock<float> block (buffer);
    REQUIRE_THAT (block, melatonin::isValidAudio());   // no NaN/Inf
    REQUIRE_FALSE (daw::render_test::hasNonFinite (buffer));
    REQUIRE_THAT (block, melatonin::isFilled());        // not silent

    // Skip ~5 ms at each edge so transport ramp-in/out doesn't bias the level.
    const int edgeSkip = (int) (0.005 * readSampleRate);

    // (a) RMS level matches the configured 0.5-amplitude sine.
    const float measuredRmsDbfs = daw::render_test::rmsDbfs (buffer, 0, edgeSkip);

    // (b) dominant FFT bin == 440 Hz within bin resolution.
    const auto peak = daw::render_test::dominantFrequency (buffer, 0, readSampleRate);

    // Human-readable diagnostics (sparkline of the first cycles + the numbers),
    // emitted via WARN so they always show in the run output, not only on failure.
    {
        juce::AudioBuffer<float> head (buffer.getArrayOfWritePointers(), 1,
                                       juce::jmin (buffer.getNumSamples(), 220));
        WARN ("[render] sine sparkline (first ~2 cycles): "
              << melatonin::sparkline (head).toStdString());
        WARN ("[render] measured RMS = " << measuredRmsDbfs << " dBFS"
              << " (expected " << expectedRmsDbfs << " +/- " << rmsToleranceDb << ")");
        WARN ("[render] dominant bin " << peak.binIndex
              << " -> " << peak.frequencyHz << " Hz"
              << " (expected " << frequencyHz
              << ", bin resolution +/- " << peak.binResolutionHz << " Hz)");
    }

    CHECK_THAT (measuredRmsDbfs, WithinAbs (expectedRmsDbfs, rmsToleranceDb));
    CHECK_THAT (peak.frequencyHz, WithinAbs (frequencyHz, peak.binResolutionHz));
}
