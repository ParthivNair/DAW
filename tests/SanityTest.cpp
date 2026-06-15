// Phase 0 sanity test: proves the build skeleton links and runs.
//  - daw_core (engine glue) is reachable and reports versions.
//  - melatonin_test_helpers Catch2 matchers + melatonin_audio_sparklines compile
//    and link against the engine's JUCE.
//  - Catch2 (with main) registers tests for CTest (catch_discover_tests).

#include <catch2/catch_test_macros.hpp>

#include <juce_dsp/juce_dsp.h>
#include <melatonin_audio_sparklines/melatonin_audio_sparklines.h>
#include <melatonin_test_helpers/melatonin_test_helpers.h>

#include "engine/EngineInfo.h"

TEST_CASE ("daw_core engine glue is linkable", "[sanity]")
{
    const auto info = daw::engineInfoString();
    REQUIRE (info.isNotEmpty());
    REQUIRE (info.contains ("JUCE"));
}

TEST_CASE ("Rubber Band is compiled into this build", "[sanity][rubberband]")
{
    // Phase 0 wires Rubber Band in; SoundTouch stays as the A/B fallback.
    REQUIRE (daw::rubberBandBuiltIn());
}

TEST_CASE ("melatonin test helpers + sparklines work on a JUCE AudioBlock", "[sanity][audio]")
{
    juce::AudioBuffer<float> buffer (1, 64);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample (0, i, std::sin (juce::MathConstants<float>::twoPi * static_cast<float> (i) / 32.0f));

    juce::dsp::AudioBlock<float> block (buffer);

    // melatonin_test_helpers Catch2 matchers (use sparklines in their failure output).
    REQUIRE_THAT (block, melatonin::isValidAudio());
    REQUIRE_THAT (block, melatonin::isEqualTo (block));
}
