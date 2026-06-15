#pragma once

#include <juce_core/juce_core.h>
#include <memory>

namespace tracktion
{
inline namespace engine
{
    class Edit;
    class Engine;
} // namespace engine
} // namespace tracktion

namespace daw
{
/** Parameters for the canonical "single 440 Hz sine" Edit.

    This is the one place the first-sound milestone's signal is defined, so the
    render test and the GUI demo session render *the same* thing. Defaults give a
    pure sine of linear amplitude 0.5 (-6.02 dBFS peak, -9.03 dBFS RMS) on one
    audio track, master + track volume at unity so the tone plugin's level is the
    only gain in the chain. */
struct SineToneSpec
{
    double frequencyHz  = 440.0; ///< Tone Generator frequency.
    float levelLinear   = 0.5f;  ///< Tone Generator level (linear amplitude of the sine).
    double durationSecs = 1.5;   ///< Length of the tone region.
};

/** Builds a fresh Edit (single audio track) carrying a ToneGeneratorPlugin set to
    a sine at the requested frequency/level. GUI-free; safe from tests and from
    daw_core. The returned Edit owns everything; the plugin lives on track 0.

    Why the Tone Generator: it is the simplest deterministic engine source that
    produces audio with no input (producesAudioWhenNoAudioInput()/isSynth()), and
    its output is exactly `level * sin(2*pi*f*t)` — no clip, file, tempo or
    time-stretch in the path — which makes the render bit-stable and the expected
    RMS/peak trivial to state. */
std::unique_ptr<tracktion::engine::Edit> buildSineToneEdit (tracktion::engine::Engine&,
                                                            const SineToneSpec& = {});
} // namespace daw
