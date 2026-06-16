#include "TransportModel.h"

#include "TimelineViewModel.h" // barsBeatsText / minSecText

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
TransportModel::TransportModel (te::Edit& e)
    : edit (e)
{
}

void TransportModel::play()
{
    auto& t = edit.getTransport();
    t.ensureContextAllocated();
    t.play (false);
}

void TransportModel::stop()
{
    // discardRecordings=false, clearDevices=false: a normal stop that leaves the playhead.
    // (Unlike the Phase 0 tone demo we do not need clearDevices — clips follow the playhead
    // and fall silent on stop; only the continuous tone generator needed the teardown.)
    edit.getTransport().stop (false, false);
}

void TransportModel::togglePlay()
{
    if (isPlaying())
        stop();
    else
        play();
}

bool TransportModel::isPlaying() const
{
    return edit.getTransport().isPlaying();
}

double TransportModel::positionSecs() const
{
    return edit.getTransport().getPosition().inSeconds();
}

void TransportModel::setPositionSecs (double secs)
{
    edit.getTransport().setPosition (tracktion::TimePosition::fromSeconds (secs));
}

void TransportModel::setLoopRangeSecs (double startSecs, double endSecs)
{
    edit.getTransport().setLoopRange ({ tracktion::TimePosition::fromSeconds (startSecs),
                                        tracktion::TimePosition::fromSeconds (endSecs) });
}

void TransportModel::setLooping (bool shouldLoop)
{
    edit.getTransport().looping = shouldLoop;
}

bool TransportModel::isLooping() const
{
    return edit.getTransport().looping;
}

juce::String TransportModel::barsBeatsString() const
{
    return barsBeatsText (edit, positionSecs());
}

juce::String TransportModel::minSecString() const
{
    return minSecText (positionSecs());
}
} // namespace daw
