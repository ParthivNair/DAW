#include "TimelineComponent.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
TimelineComponent::TimelineComponent (te::Edit& e, TimelineViewModel& vm)
    : edit (e), viewModel (vm)
{
    rebuildClips();
}

TimelineComponent::~TimelineComponent() = default;

int TimelineComponent::numTracks() const
{
    return juce::jmax (1, te::getAudioTracks (edit).size());
}

int TimelineComponent::trackHeight() const
{
    return juce::jmax (1, getHeight() / numTracks());
}

void TimelineComponent::rebuildClips()
{
    clips.clear();
    clipTrackIndex.clear();

    const auto tracks = te::getAudioTracks (edit);
    for (int ti = 0; ti < tracks.size(); ++ti)
    {
        for (auto* clip : tracks[ti]->getClips())
        {
            if (auto* wave = dynamic_cast<te::WaveAudioClip*> (clip))
            {
                auto* cc = clips.add (new ClipComponent (edit, *wave, viewModel));
                clipTrackIndex.add (ti);
                addAndMakeVisible (cc);
            }
        }
    }

    relayout();
}

void TimelineComponent::relayout()
{
    const int th = trackHeight();

    for (int i = 0; i < clips.size(); ++i)
    {
        auto& cc       = *clips[i];
        const auto pos = cc.clip().getPosition();
        const int x1   = (int) std::lround (viewModel.timeToX (pos.getStart().inSeconds()));
        const int x2   = (int) std::lround (viewModel.timeToX (pos.getEnd().inSeconds()));
        const int y    = clipTrackIndex[i] * th;
        cc.setBounds (x1, y, juce::jmax (2, x2 - x1), th);
    }
}

void TimelineComponent::resized()
{
    relayout();
}

void TimelineComponent::setPlayheadSecs (double secs)
{
    if (! juce::approximatelyEqual (playhead, secs))
    {
        playhead = secs;
        repaint();
    }
}

void TimelineComponent::paint (juce::Graphics& g)
{
    const int th = trackHeight();
    const int n  = numTracks();

    // Track lane backgrounds (alternating).
    for (int ti = 0; ti < n; ++ti)
    {
        const auto lane = juce::Rectangle<int> (0, ti * th, getWidth(), th);
        g.setColour ((ti % 2 == 0) ? juce::Colour (0xff2b2b2b) : juce::Colour (0xff262626));
        g.fillRect (lane);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawHorizontalLine ((ti + 1) * th - 1, 0.0f, (float) getWidth());
    }

    // Playhead.
    const float px = (float) viewModel.timeToX (playhead);
    if (px >= 0.0f && px <= (float) getWidth())
    {
        g.setColour (juce::Colours::orange);
        g.drawVerticalLine ((int) std::lround (px), 0.0f, (float) getHeight());
    }
}
} // namespace daw
