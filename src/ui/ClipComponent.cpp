#include "ClipComponent.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
ClipComponent::ClipComponent (te::Edit& e, te::WaveAudioClip& c, const TimelineViewModel& vm)
    : edit (e), waveClip (c), viewModel (vm)
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

ClipComponent::DragMode ClipComponent::modeForX (int localX) const
{
    if (localX <= edgeZonePx)
        return DragMode::trimLeft;
    if (localX >= getWidth() - edgeZonePx)
        return DragMode::trimRight;
    return DragMode::move;
}

juce::MouseCursor ClipComponent::getMouseCursor()
{
    switch (modeForX (getMouseXYRelative().x))
    {
        case DragMode::trimLeft:
        case DragMode::trimRight:
            return juce::MouseCursor::LeftRightResizeCursor;
        case DragMode::move:
        case DragMode::none:
        default:
            return juce::MouseCursor::NormalCursor;
    }
}

void ClipComponent::mouseDown (const juce::MouseEvent& e)
{
    if (onSelected != nullptr)
        onSelected (*this);

    if (e.mods.isPopupMenu())
    {
        dragMode = DragMode::none;
        if (onRightClicked != nullptr)
            onRightClicked (*this, e.getScreenPosition());
        return;
    }

    dragMode        = modeForX (e.x);
    dragStartBounds = getBounds();
}

void ClipComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::none)
        return;

    constexpr int minW = 4;
    const int dx       = e.getDistanceFromDragStartX();

    switch (dragMode)
    {
        case DragMode::move:
            setBounds (dragStartBounds.translated (dx, 0));
            break;

        case DragMode::trimLeft:
        {
            const int right = dragStartBounds.getRight();
            const int newX  = juce::jmin (right - minW, dragStartBounds.getX() + dx);
            setBounds (newX, dragStartBounds.getY(), right - newX, dragStartBounds.getHeight());
            break;
        }

        case DragMode::trimRight:
        {
            const int newW = juce::jmax (minW, dragStartBounds.getWidth() + dx);
            setBounds (dragStartBounds.getX(), dragStartBounds.getY(), newW, dragStartBounds.getHeight());
            break;
        }

        case DragMode::none:
        default:
            break;
    }
}

void ClipComponent::mouseUp (const juce::MouseEvent&)
{
    const auto mode = dragMode;
    dragMode        = DragMode::none;

    switch (mode)
    {
        case DragMode::move:
            if (onMoved != nullptr)
                onMoved (*this, viewModel.xToTime (getX()));
            break;
        case DragMode::trimLeft:
            if (onTrimLeftTo != nullptr)
                onTrimLeftTo (*this, viewModel.xToTime (getX()));
            break;
        case DragMode::trimRight:
            if (onTrimRightTo != nullptr)
                onTrimRightTo (*this, viewModel.xToTime (getRight()));
            break;
        case DragMode::none:
        default:
            break;
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
    // us); until then there's simply nothing to draw over the body.
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

    // Fade ramps: shade a triangle over the faded-in start and faded-out end, sized by
    // the fade length as a fraction of the clip length.
    {
        const double clipLen = waveClip.getPosition().getLength().inSeconds();
        if (clipLen > 0.0)
        {
            const float w  = bounds.getWidth();
            const float h  = bounds.getHeight();
            const float fi = (float) (waveClip.getFadeIn().inSeconds() / clipLen) * w;
            const float fo = (float) (waveClip.getFadeOut().inSeconds() / clipLen) * w;

            g.setColour (juce::Colours::black.withAlpha (0.45f));
            if (fi > 1.0f)
            {
                juce::Path p;
                p.startNewSubPath (0.0f, 0.0f);
                p.lineTo (fi, 0.0f);
                p.lineTo (0.0f, h);
                p.closeSubPath();
                g.fillPath (p);
            }
            if (fo > 1.0f)
            {
                juce::Path p;
                p.startNewSubPath (w, 0.0f);
                p.lineTo (w, h);
                p.lineTo (w - fo, 0.0f);
                p.closeSubPath();
                g.fillPath (p);
            }
        }
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
