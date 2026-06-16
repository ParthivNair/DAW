#include <juce_gui_extra/juce_gui_extra.h>

#include <tracktion_engine/tracktion_engine.h>

#include "engine/ArrangementEdit.h"
#include "engine/EngineInfo.h"
#include "engine/TimelineViewModel.h"
#include "ui/TimeRulerComponent.h"
#include "ui/TimelineComponent.h"

namespace te = tracktion::engine;

namespace daw
{
//==============================================================================
/** Phase 1 timeline shell: a headless arrangement Edit shown through the daw_ui
    timeline components (ruler + track lanes + playhead). Transport, menus, drag-drop
    import and clip interactions arrive in Chunk 8; this chunk proves the components
    render in the real app over a real Edit. */
class MainComponent final : public juce::Component
{
public:
    MainComponent()
    {
        engine = std::make_unique<te::Engine> ("EZStudio");
        edit   = buildArrangementEdit (*engine, 4, EditPurpose::livePlayback);

        viewModel.setVisibleTimeRange (0.0, 16.0);

        ruler    = std::make_unique<TimeRulerComponent> (*edit, viewModel);
        timeline = std::make_unique<TimelineComponent> (*edit, viewModel);

        addAndMakeVisible (*ruler);
        addAndMakeVisible (*timeline);

        setSize (1000, 600);
    }

    ~MainComponent() override
    {
        timeline = nullptr;
        ruler    = nullptr;
        edit     = nullptr;
        engine   = nullptr;
    }

    void resized() override
    {
        auto r = getLocalBounds();
        viewModel.setWidthPixels (r.getWidth());
        ruler->setBounds (r.removeFromTop (24));
        timeline->setBounds (r);
        ruler->repaint();
        timeline->relayout();
    }

private:
    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<te::Edit> edit;
    TimelineViewModel viewModel;
    std::unique_ptr<TimeRulerComponent> ruler;
    std::unique_ptr<TimelineComponent> timeline;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

//==============================================================================
class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow()
        : juce::DocumentWindow ("EZStudio", juce::Colours::darkgrey, juce::DocumentWindow::allButtons)
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

    void shutdown() override { mainWindow = nullptr; }

    void systemRequestedQuit() override { quit(); }

private:
    std::unique_ptr<MainWindow> mainWindow;
};
} // namespace daw

START_JUCE_APPLICATION (daw::EZStudioApplication)
