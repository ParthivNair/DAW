#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace daw
{
class TransportModel;

/** The transport bar: play / stop / loop buttons and a bars:beats + min:sec readout,
    bound to a TransportModel. While playing it ticks a Timer to refresh the readout and
    publish the playhead position (onPlayheadMoved) so the timeline can draw + auto-follow
    the playhead. */
class TransportBarComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    explicit TransportBarComponent (TransportModel&);
    ~TransportBarComponent() override;

    /** Called ~30x/s while playing (and once on stop) with the current playhead position
        and whether the transport is playing — the host wires this to the timeline. */
    std::function<void (double playheadSecs, bool isPlaying)> onPlayheadMoved;

    /** Refreshes button states + readout from the model (call after external changes). */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateReadout();

    TransportModel& transport;
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton loopButton { "Loop" };
    juce::Label readout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBarComponent)
};
} // namespace daw
