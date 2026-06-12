#include <juce_gui_extra/juce_gui_extra.h>

#include "engine/EngineInfo.h"

namespace daw
{
//==============================================================================
/** Minimal empty main window. Phase 0 placeholder — the real UI lands later. */
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
        centreWithSize (800, 600);
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
    const juce::String getApplicationName() override    { return "EZStudio"; }
    const juce::String getApplicationVersion() override { return "0.0.1"; }
    bool moreThanOneInstanceAllowed() override          { return false; }

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
