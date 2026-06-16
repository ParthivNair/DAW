#pragma once

#include "ClipComponent.h"
#include "engine/TimelineViewModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace tracktion
{
inline namespace engine
{
    class Edit;
} // namespace engine
} // namespace tracktion

namespace daw
{
/** The track-lanes area of the timeline: draws the lane backgrounds and the playhead, and
    hosts a ClipComponent per clip positioned from the shared TimelineViewModel. Driven by
    the model — call rebuildClips() after the arrangement changes and relayout()/repaint()
    after a zoom/scroll. Interactions (select/drag/trim) are added in Chunk 8. */
class TimelineComponent final : public juce::Component
{
public:
    TimelineComponent (tracktion::engine::Edit&, TimelineViewModel&);
    ~TimelineComponent() override;

    /** Recreates the ClipComponents from the current arrangement. */
    void rebuildClips();

    /** Repositions existing clips for the current view (call after zoom/scroll/resize). */
    void relayout();

    void setPlayheadSecs (double secs);
    double playheadSecs() const noexcept { return playhead; }

    int numTracks() const;
    int trackHeight() const;

    /** Clip components, in creation order (for tests / selection). */
    const juce::OwnedArray<ClipComponent>& clipComponents() const noexcept { return clips; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int trackIndexOf (int clipComponentIndex) const { return clipTrackIndex[clipComponentIndex]; }

    tracktion::engine::Edit& edit;
    TimelineViewModel& viewModel;
    juce::OwnedArray<ClipComponent> clips;
    juce::Array<int> clipTrackIndex; // parallel to `clips`
    double playhead = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
};
} // namespace daw
