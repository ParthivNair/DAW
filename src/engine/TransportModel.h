#pragma once

#include <juce_core/juce_core.h>

namespace tracktion
{
inline namespace engine
{
    class Edit;
} // namespace engine
} // namespace tracktion

namespace daw
{
/** Transport facade for the timeline UI's transport bar: play / stop / loop / position,
    plus the bars:beats and min:sec readouts. GUI-free (daw_core); wraps the Edit's
    TransportControl so the UI never touches engine types directly.

    The Edit must have been built for live playback (EditPurpose::livePlayback ->
    Edit::forEditing) for play() to drive a device. */
class TransportModel
{
public:
    explicit TransportModel (tracktion::engine::Edit&);

    /** Allocates the playback context (if needed) and starts playing. */
    void play();
    /** Stops playback (leaves the playhead where it is). */
    void stop();
    /** Convenience: stop if playing, else play. */
    void togglePlay();
    bool isPlaying() const;

    /** Playhead position in seconds. */
    double positionSecs() const;
    void setPositionSecs (double secs);

    /** Sets the loop region (seconds) and whether looping is on. */
    void setLoopRangeSecs (double startSecs, double endSecs);
    void setLooping (bool shouldLoop);
    bool isLooping() const;

    /** Bars|beats readout at the current playhead position (e.g. "2|3"). */
    juce::String barsBeatsString() const;
    /** min:sec.ms readout at the current playhead position (e.g. "0:02.500"). */
    juce::String minSecString() const;

private:
    tracktion::engine::Edit& edit;
};
} // namespace daw
