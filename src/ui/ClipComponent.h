#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
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
    the parent TimelineComponent from the TimelineViewModel — the clip itself is purely a
    painter and (in Chunk 8) a hit-target; it never owns timeline geometry. */
class ClipComponent final : public juce::Component
{
public:
    ClipComponent (tracktion::engine::Edit&, tracktion::engine::WaveAudioClip&);
    ~ClipComponent() override;

    void paint (juce::Graphics&) override;

    tracktion::engine::WaveAudioClip& clip() noexcept { return waveClip; }

    void setSelected (bool shouldBeSelected);
    bool isSelected() const noexcept { return selected; }

private:
    tracktion::engine::Edit& edit;
    tracktion::engine::WaveAudioClip& waveClip;
    std::unique_ptr<tracktion::engine::SmartThumbnail> thumbnail;
    bool selected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipComponent)
};
} // namespace daw
