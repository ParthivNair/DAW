#include "ArrangementView.h"

#include "ClipComponent.h"
#include "engine/ClipImporter.h"
#include "engine/ClipOps.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
namespace
{
    bool isAudioFile (const juce::File& f)
    {
        return f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a;caf");
    }
} // namespace

ArrangementView::ArrangementView (te::Edit& e, TimelineViewModel& vm)
    : edit (e), viewModel (vm), ruler (e, vm), timeline (e, vm)
{
    addAndMakeVisible (ruler);
    addAndMakeVisible (timeline);
    setWantsKeyboardFocus (true);
    rebuild();
}

ArrangementView::~ArrangementView() = default;

void ArrangementView::rebuild()
{
    timeline.rebuildClips();
    selected = nullptr;
    for (auto* cc : timeline.clipComponents())
        wireClip (*cc);
    relayoutAll();
}

void ArrangementView::wireClip (ClipComponent& cc)
{
    cc.onSelected = [this] (ClipComponent& c)
    { selectClip (&c); };
    cc.onMoved = [this] (ClipComponent& c, double s)
    { moveClipTo (c, s); };
    cc.onTrimLeftTo = [this] (ClipComponent& c, double s)
    { trimClipLeftTo (c, s); };
    cc.onTrimRightTo = [this] (ClipComponent& c, double t)
    { trimClipRightTo (c, t); };
    cc.onRightClicked = [this] (ClipComponent& c, juce::Point<int> p)
    {
        selectClip (&c);
        showClipMenu (c, p);
    };
}

void ArrangementView::relayoutAll()
{
    timeline.relayout();
    timeline.repaint();
    ruler.repaint();
}

double ArrangementView::maybeSnap (double secs) const
{
    return snap ? snapSecondsToBeat (edit, secs) : secs;
}

void ArrangementView::selectClip (ClipComponent* cc)
{
    if (selected == cc)
        return;
    if (selected != nullptr)
        selected->setSelected (false);
    selected = cc;
    if (selected != nullptr)
        selected->setSelected (true);
}

void ArrangementView::moveClipTo (ClipComponent& cc, double newStartSecs)
{
    moveClip (cc.clip(), juce::jmax (0.0, maybeSnap (newStartSecs)));
    relayoutAll();
    if (onEdited != nullptr)
        onEdited();
}

void ArrangementView::trimClipLeftTo (ClipComponent& cc, double newStartSecs)
{
    const double end     = cc.clip().getPosition().getEnd().inSeconds();
    const double clamped = juce::jlimit (0.0, end - 0.02, maybeSnap (newStartSecs));
    daw::trimClipLeftTo (cc.clip(), clamped);
    relayoutAll();
    if (onEdited != nullptr)
        onEdited();
}

void ArrangementView::trimClipRightTo (ClipComponent& cc, double newEndSecs)
{
    const double start  = cc.clip().getPosition().getStart().inSeconds();
    const double newLen = juce::jmax (0.02, maybeSnap (newEndSecs) - start);
    setClipLength (cc.clip(), newLen);
    relayoutAll();
    if (onEdited != nullptr)
        onEdited();
}

void ArrangementView::setFadeInOnSelected (double secs)
{
    if (selected == nullptr)
        return;
    setClipFadeIn (selected->clip(), secs);
    relayoutAll();
    if (onEdited != nullptr)
        onEdited();
}

void ArrangementView::setFadeOutOnSelected (double secs)
{
    if (selected == nullptr)
        return;
    setClipFadeOut (selected->clip(), secs);
    relayoutAll();
    if (onEdited != nullptr)
        onEdited();
}

void ArrangementView::clearFadesOnSelected()
{
    if (selected == nullptr)
        return;
    setClipFadeIn (selected->clip(), 0.0);
    setClipFadeOut (selected->clip(), 0.0);
    relayoutAll();
    if (onEdited != nullptr)
        onEdited();
}

void ArrangementView::nudgeGainOnSelected (float deltaDb)
{
    if (selected == nullptr)
        return;
    setClipGainDb (selected->clip(), selected->clip().getGainDB() + deltaDb);
    relayoutAll();
    if (onEdited != nullptr)
        onEdited();
}

void ArrangementView::deleteSelected()
{
    if (selected == nullptr)
        return;
    deleteClip (selected->clip());
    selected = nullptr;
    rebuild();
    if (onEdited != nullptr)
        onEdited();
}

ClipComponent* ArrangementView::importFileAt (const juce::File& file, int trackIndex, double atSecs)
{
    auto tracks = te::getAudioTracks (edit);
    if (tracks.isEmpty())
        return nullptr;

    trackIndex = juce::jlimit (0, tracks.size() - 1, trackIndex);
    auto* clip = importAudioFileAsClip (*tracks[trackIndex], file, juce::jmax (0.0, atSecs));
    rebuild();
    if (onEdited != nullptr)
        onEdited();

    ClipComponent* added = nullptr;
    if (clip != nullptr)
        for (auto* cc : timeline.clipComponents())
            if (&cc->clip() == clip)
                added = cc;

    selectClip (added);
    return added;
}

void ArrangementView::setPlayheadSecs (double secs, bool following)
{
    timeline.setPlayheadSecs (secs);

    if (following)
    {
        const double oldStart = viewModel.viewStartSecs();
        viewModel.followPlayhead (secs);
        if (! juce::approximatelyEqual (viewModel.viewStartSecs(), oldStart))
            relayoutAll();
    }
}

int ArrangementView::trackIndexAtY (int y) const
{
    const int laneY = y - rulerHeight;
    if (laneY < 0)
        return 0;
    return juce::jlimit (0, timeline.numTracks() - 1, laneY / juce::jmax (1, timeline.trackHeight()));
}

void ArrangementView::showClipMenu (ClipComponent&, juce::Point<int> screenPos)
{
    juce::PopupMenu m;
    m.addItem (1, "Delete");
    m.addSeparator();
    m.addItem (2, "Fade in (0.1 s)");
    m.addItem (3, "Fade out (0.1 s)");
    m.addItem (4, "Clear fades");
    m.addSeparator();
    m.addItem (5, "Gain +3 dB");
    m.addItem (6, "Gain -3 dB");

    m.showMenuAsync (juce::PopupMenu::Options {}.withTargetScreenArea (
                         { screenPos.x, screenPos.y, 1, 1 }),
                     [this] (int r)
                     {
                         switch (r)
                         {
                             case 1:
                                 deleteSelected();
                                 break;
                             case 2:
                                 setFadeInOnSelected (0.1);
                                 break;
                             case 3:
                                 setFadeOutOnSelected (0.1);
                                 break;
                             case 4:
                                 clearFadesOnSelected();
                                 break;
                             case 5:
                                 nudgeGainOnSelected (3.0f);
                                 break;
                             case 6:
                                 nudgeGainOnSelected (-3.0f);
                                 break;
                             default:
                                 break;
                         }
                     });
}

void ArrangementView::resized()
{
    auto r = getLocalBounds();
    viewModel.setWidthPixels (r.getWidth());
    ruler.setBounds (r.removeFromTop (rulerHeight));
    timeline.setBounds (r);
    relayoutAll();
}

bool ArrangementView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelected();
        return true;
    }
    return false;
}

void ArrangementView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double dy     = (double) wheel.deltaY;
        const double factor = dy >= 0.0 ? 1.0 + dy : 1.0 / (1.0 - dy);
        viewModel.zoomBy (factor, (double) e.position.x);
    }
    else
    {
        viewModel.scrollByPixels (-((double) wheel.deltaX + (double) wheel.deltaY) * 120.0);
    }
    relayoutAll();
}

bool ArrangementView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (isAudioFile (juce::File (f)))
            return true;
    return false;
}

void ArrangementView::filesDropped (const juce::StringArray& files, int x, int y)
{
    const int track = trackIndexAtY (y);
    double at       = juce::jmax (0.0, viewModel.xToTime (x));

    for (const auto& f : files)
    {
        juce::File file (f);
        if (! isAudioFile (file))
            continue;

        if (auto* cc = importFileAt (file, track, at))
            at = cc->clip().getPosition().getEnd().inSeconds(); // lay subsequent drops end-to-end
    }
}
} // namespace daw
