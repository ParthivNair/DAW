#pragma once

#include "TimeRulerComponent.h"
#include "TimelineComponent.h"
#include "engine/TimelineViewModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace tracktion
{
inline namespace engine
{
    class Edit;
} // namespace engine
} // namespace tracktion

namespace daw
{
class ClipComponent;

/** The arrangement editor: a time ruler over the track lanes, with all the timeline
    interactions — drag-and-drop audio import from Finder, clip select / drag-move (snapped)
    / edge-trim / right-click (delete / fades / gain), mouse-wheel zoom & scroll, and the
    auto-following playhead. Every model mutation routes through ClipOps/ClipImporter (so it
    is undoable) and then relays out from the model. The public gesture methods double as the
    seams the Chunk-8 gesture test drives without synthesising raw mouse events. */
class ArrangementView final : public juce::Component,
                              public juce::FileDragAndDropTarget
{
public:
    ArrangementView (tracktion::engine::Edit&, TimelineViewModel&);
    ~ArrangementView() override;

    /** Rebuilds clip components from the edit and re-wires their callbacks. Call after an
        import/delete or loading a different edit. */
    void rebuild();

    //== Selection ===========================================================
    void selectClip (ClipComponent*);
    ClipComponent* selectedClip() const noexcept { return selected; }
    void deleteSelected();

    //== Gestures (also the test seams) — times in seconds, snapped if snap is on =========
    void moveClipTo (ClipComponent&, double newStartSecs);
    void trimClipLeftTo (ClipComponent&, double newStartSecs);
    void trimClipRightTo (ClipComponent&, double newEndSecs);
    void setFadeInOnSelected (double secs);
    void setFadeOutOnSelected (double secs);
    void clearFadesOnSelected();
    void nudgeGainOnSelected (float deltaDb);

    /** Imports an audio file onto a track at a time; rebuilds. Returns the new clip. */
    ClipComponent* importFileAt (const juce::File&, int trackIndex, double atSecs);

    void setSnapEnabled (bool shouldSnap) { snap = shouldSnap; }
    bool isSnapEnabled() const noexcept { return snap; }

    /** Moves the drawn playhead and, while playing, auto-follows it. */
    void setPlayheadSecs (double secs, bool following);

    /** Fired after any change that the host may want to react to (e.g. mark dirty). */
    std::function<void()> onEdited;

    //== Component / FileDragAndDropTarget ===================================
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void wireClip (ClipComponent&);
    void relayoutAll();
    double maybeSnap (double secs) const;
    int trackIndexAtY (int y) const;
    void showClipMenu (ClipComponent&, juce::Point<int> screenPos);

    static constexpr int rulerHeight = 24;

    tracktion::engine::Edit& edit;
    TimelineViewModel& viewModel;
    TimeRulerComponent ruler;
    TimelineComponent timeline;
    ClipComponent* selected = nullptr;
    bool snap               = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementView)
};
} // namespace daw
