#pragma once

#include "engine/TimelineViewModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace tracktion
{
inline namespace engine
{
    class Edit;
    class WaveAudioClip;
    class SmartThumbnail;
} // namespace engine
} // namespace tracktion

namespace daw
{
/** A single wave clip on the timeline: draws the clip body, name, fade ramps, selection
    highlight, and the waveform (via the engine's SmartThumbnail). Position/size are set by
    the parent TimelineComponent from the TimelineViewModel.

    Handles its own mouse: a body drag moves the clip; a drag near the left/right edge trims
    that edge. It previews by moving its own bounds during the drag and reports the final
    time(s) to the owning view through the callbacks below (which commit via ClipOps under
    one undo transaction and relayout from the model). A popup-modifier click reports a
    right-click instead. The clip never mutates the model itself. */
class ClipComponent final : public juce::Component
{
public:
    ClipComponent (tracktion::engine::Edit&, tracktion::engine::WaveAudioClip&, const TimelineViewModel&);
    ~ClipComponent() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::MouseCursor getMouseCursor() override;

    tracktion::engine::WaveAudioClip& clip() noexcept { return waveClip; }

    void setSelected (bool shouldBeSelected);
    bool isSelected() const noexcept { return selected; }

    // Set by the owning ArrangementView; all times are in seconds on the edit timeline.
    std::function<void (ClipComponent&)> onSelected;
    std::function<void (ClipComponent&, double newStartSecs)> onMoved;      // body drag
    std::function<void (ClipComponent&, double newStartSecs)> onTrimLeftTo; // left-edge drag
    std::function<void (ClipComponent&, double newEndSecs)> onTrimRightTo;  // right-edge drag
    std::function<void (ClipComponent&, juce::Point<int> screenPos)> onRightClicked;

private:
    enum class DragMode
    {
        none,
        move,
        trimLeft,
        trimRight
    };

    DragMode modeForX (int localX) const;

    tracktion::engine::Edit& edit;
    tracktion::engine::WaveAudioClip& waveClip;
    const TimelineViewModel& viewModel;
    std::unique_ptr<tracktion::engine::SmartThumbnail> thumbnail;
    bool selected = false;

    DragMode dragMode = DragMode::none;
    juce::Rectangle<int> dragStartBounds;
    static constexpr int edgeZonePx = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipComponent)
};
} // namespace daw
