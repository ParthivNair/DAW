#include "TransportBarComponent.h"

#include "engine/TransportModel.h"

namespace daw
{
TransportBarComponent::TransportBarComponent (TransportModel& t)
    : transport (t)
{
    playButton.onClick = [this]
    {
        transport.play();
        startTimerHz (30);
        refresh();
    };
    stopButton.onClick = [this]
    {
        transport.stop();
        stopTimer();
        updateReadout();
        if (onPlayheadMoved != nullptr)
            onPlayheadMoved (transport.positionSecs(), false);
        refresh();
    };
    loopButton.setClickingTogglesState (true);
    loopButton.setToggleState (transport.isLooping(), juce::dontSendNotification);
    loopButton.onClick = [this]
    { transport.setLooping (loopButton.getToggleState()); };

    readout.setJustificationType (juce::Justification::centredRight);
    readout.setFont (juce::FontOptions (16.0f, juce::Font::bold));

    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (loopButton);
    addAndMakeVisible (readout);

    updateReadout();
}

TransportBarComponent::~TransportBarComponent()
{
    stopTimer();
}

void TransportBarComponent::refresh()
{
    playButton.setEnabled (! transport.isPlaying());
    loopButton.setToggleState (transport.isLooping(), juce::dontSendNotification);
}

void TransportBarComponent::updateReadout()
{
    readout.setText (transport.barsBeatsString() + "    " + transport.minSecString(),
                     juce::dontSendNotification);
}

void TransportBarComponent::timerCallback()
{
    updateReadout();
    if (onPlayheadMoved != nullptr)
        onPlayheadMoved (transport.positionSecs(), transport.isPlaying());

    // If playback stopped on its own (e.g. reached the end without looping), tidy up.
    if (! transport.isPlaying())
    {
        stopTimer();
        refresh();
    }
}

void TransportBarComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1c1c));
}

void TransportBarComponent::resized()
{
    auto r = getLocalBounds().reduced (6, 6);
    playButton.setBounds (r.removeFromLeft (70));
    r.removeFromLeft (4);
    stopButton.setBounds (r.removeFromLeft (70));
    r.removeFromLeft (4);
    loopButton.setBounds (r.removeFromLeft (70));
    readout.setBounds (r.removeFromRight (220));
}
} // namespace daw
