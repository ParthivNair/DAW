#include "TimeRulerComponent.h"

#include <tracktion_engine/tracktion_engine.h>

#include <cmath>

namespace te = tracktion::engine;

namespace daw
{
TimeRulerComponent::TimeRulerComponent (te::Edit& e, const TimelineViewModel& vm)
    : edit (e), viewModel (vm)
{
}

void TimeRulerComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff222222));

    auto& ts               = edit.tempoSequence;
    const double viewStart = viewModel.viewStartSecs();
    const double viewEnd   = viewModel.viewEndSecs();

    const auto startBeats = ts.toBeats (tracktion::TimePosition::fromSeconds (viewStart)).inBeats();
    const auto endBeats   = ts.toBeats (tracktion::TimePosition::fromSeconds (viewEnd)).inBeats();
    const int numerator   = juce::jmax (1, ts.toBarsAndBeats (tracktion::TimePosition::fromSeconds (viewStart)).numerator);

    const int firstBeat = (int) std::floor (startBeats);
    const int lastBeat  = (int) std::ceil (endBeats);

    // Guard against absurd zoom-outs producing thousands of lines.
    if (lastBeat - firstBeat > 4096)
        return;

    const float h = (float) getHeight();

    for (int beat = firstBeat; beat <= lastBeat; ++beat)
    {
        if (beat < 0)
            continue;

        const double t = ts.toTime (tracktion::BeatPosition::fromBeats ((double) beat)).inSeconds();
        const float x  = (float) viewModel.timeToX (t);
        if (x < -1.0f || x > (float) getWidth() + 1.0f)
            continue;

        const bool isBarStart = (beat % numerator) == 0;

        g.setColour (isBarStart ? juce::Colours::white.withAlpha (0.55f)
                                : juce::Colours::white.withAlpha (0.18f));
        g.drawVerticalLine ((int) std::lround (x), isBarStart ? h * 0.35f : h * 0.65f, h);

        if (isBarStart)
        {
            const int barNumber = beat / numerator + 1; // 1-based bar numbers
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (juce::String (barNumber), (int) x + 3, 1, 40, 14,
                        juce::Justification::topLeft, false);
        }
    }
}
} // namespace daw
