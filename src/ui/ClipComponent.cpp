#include "ClipComponent.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
ClipComponent::ClipComponent (te::Edit& e, te::WaveAudioClip& c)
    : edit (e), waveClip (c)
{
    // SmartThumbnail repaints THIS component as it finishes generating asynchronously.
    thumbnail = std::make_unique<te::SmartThumbnail> (edit.engine, c.getAudioFile(), *this, &edit);
    setInterceptsMouseClicks (true, false);
}

ClipComponent::~ClipComponent() = default;

void ClipComponent::setSelected (bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

void ClipComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    const auto base    = juce::Colour (0xff3a6ea5);
    const auto bodyTop = selected ? base.brighter (0.35f) : base;
    const auto bodyBot = bodyTop.darker (0.35f);

    g.setGradientFill ({ bodyTop, bounds.getTopLeft(), bodyBot, bounds.getBottomLeft(), false });
    g.fillRoundedRectangle (bounds, 3.0f);

    // Waveform: draw the clip's source range. The thumbnail fills in async (repainting
    // us); getProportionComplete() > 0 means there's something to show.
    if (thumbnail != nullptr)
    {
        auto wave         = getLocalBounds().reduced (2, 2);
        const auto offset = waveClip.getPosition().getOffset();
        const auto len    = waveClip.getPosition().getLength();
        const tracktion::TimeRange sourceRange { tracktion::toPosition (offset),
                                                 tracktion::toPosition (offset) + len };

        g.setColour (juce::Colours::white.withAlpha (0.8f));
        thumbnail->drawChannels (g, wave, sourceRange, 1.0f);
    }

    // Name.
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (waveClip.getName(), getLocalBounds().reduced (4, 2).removeFromTop (14),
                juce::Justification::topLeft, true);

    // Border (brighter when selected).
    g.setColour (selected ? juce::Colours::white : juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, selected ? 2.0f : 1.0f);
}
} // namespace daw
