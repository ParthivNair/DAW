#include <juce_gui_extra/juce_gui_extra.h>

#include "engine/EngineInfo.h"
#include "engine/SineDemoSession.h"

namespace daw
{
//==============================================================================
/** Phase 0 first-sound window: a single Play/Stop toggle over the 440 Hz demo
    session. All engine work lives in daw::SineDemoSession (GUI-free, daw_core);
    this component is just the JUCE shell around it. */
class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent()
    {
        playStopButton.setButtonText ("Play 440 Hz");
        playStopButton.onClick = [this]
        {
            if (session.isPlaying())
            {
                session.stop();
                playStopButton.setButtonText ("Play 440 Hz");
            }
            else
            {
                session.play();
                playStopButton.setButtonText ("Stop");
            }
        };

        deviceLabel.setJustificationType (juce::Justification::centred);

        addAndMakeVisible (playStopButton);
        addAndMakeVisible (deviceLabel);

        // First-sound milestone: start playing immediately on launch so the human
        // (or a smoke run) hears the tone without any interaction.
        session.play();
        playStopButton.setButtonText ("Stop");

        // The device list rebuilds asynchronously, so the chosen output may not be
        // resolvable yet at construction time. Refresh the label/log once it has
        // settled (and stop polling once it reports a real device).
        updateDeviceLabel();
        startTimer (250);

        setSize (800, 600);
    }

    ~MainComponent() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20);
        deviceLabel.setBounds (r.removeFromTop (28));
        playStopButton.setBounds (r.withSizeKeepingCentre (160, 40));
    }

private:
    void updateDeviceLabel()
    {
        const auto desc = session.outputDeviceDescription();
        deviceLabel.setText ("Output: " + desc, juce::dontSendNotification);
    }

    void timerCallback() override
    {
        updateDeviceLabel();

        // Once a real output device has been resolved, log it once and stop.
        if (! session.outputDeviceDescription().startsWith ("no "))
        {
            juce::Logger::writeToLog ("EZStudio output device: " + session.outputDeviceDescription());
            stopTimer();
        }
    }

    SineDemoSession session; // owns the engine + edit; opens the default device.
    juce::TextButton playStopButton;
    juce::Label deviceLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

//==============================================================================
/** Minimal main window hosting the first-sound MainComponent. */
class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow()
        : juce::DocumentWindow ("EZStudio",
                                juce::Colours::darkgrey,
                                juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, false);
        setContentOwned (new MainComponent(), true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

//==============================================================================
class EZStudioApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "EZStudio"; }
    const juce::String getApplicationVersion() override { return "0.0.1"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String&) override
    {
        juce::Logger::writeToLog (engineInfoString());
        mainWindow = std::make_unique<MainWindow>();
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
};
} // namespace daw

START_JUCE_APPLICATION (daw::EZStudioApplication)
