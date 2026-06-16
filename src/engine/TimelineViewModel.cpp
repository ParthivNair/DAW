#include "TimelineViewModel.h"

#include <tracktion_engine/tracktion_engine.h>

#include <cmath>

namespace te = tracktion::engine;

namespace daw
{
namespace
{
    double snapToBeatMultiple (te::Edit& edit, double secs, double beatMultiple)
    {
        auto& ts           = edit.tempoSequence;
        const double beats = ts.toBeats (tracktion::TimePosition::fromSeconds (secs)).inBeats();
        const double snapped =
            std::round (beats / beatMultiple) * beatMultiple;
        return ts.toTime (tracktion::BeatPosition::fromBeats (snapped)).inSeconds();
    }
} // namespace

double snapSecondsToBeat (te::Edit& edit, double secs)
{
    return snapToBeatMultiple (edit, secs, 1.0);
}

double snapSecondsToBar (te::Edit& edit, double secs)
{
    // Snap to the nearest whole bar = nearest multiple of (beats per bar) beats.
    const auto bb       = edit.tempoSequence.toBarsAndBeats (tracktion::TimePosition::fromSeconds (secs));
    const double perBar = juce::jmax (1, bb.numerator);
    return snapToBeatMultiple (edit, secs, perBar);
}

juce::String barsBeatsText (te::Edit& edit, double secs)
{
    const auto bb = edit.tempoSequence.toBarsAndBeats (tracktion::TimePosition::fromSeconds (secs));
    // bars / whole-beats are 0-based internally; display 1-based like a DAW.
    return juce::String (bb.bars + 1) + "|" + juce::String (bb.getWholeBeats() + 1);
}

juce::String minSecText (double secs)
{
    const bool negative  = secs < 0.0;
    const double a       = std::abs (secs);
    const int minutes    = (int) (a / 60.0);
    const double seconds = a - minutes * 60.0;

    return juce::String (negative ? "-" : "") + juce::String (minutes) + ":" + juce::String (seconds, 3).paddedLeft ('0', 6); // SS.mmm
}
} // namespace daw
