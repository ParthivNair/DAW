#include <juce_gui_extra/juce_gui_extra.h>

#include <tracktion_engine/tracktion_engine.h>

#include "engine/ArrangementRenderer.h"
#include "engine/EditUndo.h"
#include "engine/EngineInfo.h"
#include "engine/ProjectSession.h"
#include "engine/TimelineViewModel.h"
#include "engine/TransportModel.h"
#include "ui/ArrangementView.h"
#include "ui/ClipComponent.h"
#include "ui/TransportBarComponent.h"

namespace te = tracktion::engine;

namespace daw
{
//==============================================================================
/** Phase 1 timeline app: a transport bar over the arrangement editor, with a File/Edit/
    View menu wired to ProjectSession (new/open/save), ArrangementRenderer (export),
    EditUndo, and the arrangement's clip gestures. Drag audio from Finder onto a lane to
    import; drag clips to move, drag their edges to trim, right-click for fades/gain/delete;
    Play to hear it through the default output. */
class AppComponent final : public juce::Component,
                           public juce::MenuBarModel
{
public:
    AppComponent()
    {
        engine = std::make_unique<te::Engine> ("EZStudio");
        engine->getDeviceManager().initialise(); // live playback through the default device

        menuBar = std::make_unique<juce::MenuBarComponent> (this);
        addAndMakeVisible (*menuBar);

        session = std::make_unique<ProjectSession> (*engine);
        newProject();

        setSize (1100, 680);
    }

    ~AppComponent() override
    {
        teardownUI();
        session = nullptr;
        if (engine != nullptr)
            engine->getDeviceManager().closeDevices();
        engine = nullptr;
    }

    //== Layout ==============================================================
    void resized() override
    {
        auto r = getLocalBounds();
        menuBar->setBounds (r.removeFromTop (24));
        if (transportBar != nullptr)
            transportBar->setBounds (r.removeFromTop (40));
        if (arrangement != nullptr)
            arrangement->setBounds (r);
    }

    //== MenuBarModel ========================================================
    juce::StringArray getMenuBarNames() override { return { "File", "Edit", "View" }; }

    juce::PopupMenu getMenuForIndex (int index, const juce::String&) override
    {
        juce::PopupMenu m;
        if (index == 0) // File
        {
            m.addItem (newId, "New");
            m.addItem (openId, "Open...");
            m.addItem (importId, "Import Audio...", arrangement != nullptr);
            m.addSeparator();
            m.addItem (saveId, "Save");
            m.addItem (saveAsId, "Save As...");
            m.addSeparator();
            m.addItem (exportId, "Export WAV...");
        }
        else if (index == 1) // Edit
        {
            m.addItem (undoId, "Undo", arrangement != nullptr && canUndo (*session->edit()));
            m.addItem (redoId, "Redo", arrangement != nullptr && canRedo (*session->edit()));
            m.addSeparator();
            m.addItem (deleteId, "Delete Clip",
                       arrangement != nullptr && arrangement->selectedClip() != nullptr);
        }
        else if (index == 2) // View
        {
            m.addItem (snapId, "Snap to beat", true, arrangement != nullptr && arrangement->isSnapEnabled());
        }
        return m;
    }

    void menuItemSelected (int itemId, int) override
    {
        switch (itemId)
        {
            case newId:
                newProject();
                break;
            case openId:
                chooseOpen();
                break;
            case importId:
                chooseImport();
                break;
            case saveId:
                doSave();
                break;
            case saveAsId:
                chooseSaveAs();
                break;
            case exportId:
                chooseExport();
                break;
            case undoId:
                if (arrangement)
                {
                    undo (*session->edit());
                    arrangement->rebuild();
                }
                break;
            case redoId:
                if (arrangement)
                {
                    redo (*session->edit());
                    arrangement->rebuild();
                }
                break;
            case deleteId:
                if (arrangement)
                    arrangement->deleteSelected();
                break;
            case snapId:
                if (arrangement)
                    arrangement->setSnapEnabled (! arrangement->isSnapEnabled());
                break;
            default:
                break;
        }
    }

private:
    enum MenuIds
    {
        newId = 1,
        openId,
        importId,
        saveId,
        saveAsId,
        exportId,
        undoId,
        redoId,
        deleteId,
        snapId
    };

    void teardownUI()
    {
        arrangement    = nullptr;
        transportBar   = nullptr;
        transportModel = nullptr;
    }

    void buildUI()
    {
        auto* edit = session->edit();
        jassert (edit != nullptr);

        viewModel.setVisibleTimeRange (0.0, 16.0);

        transportModel = std::make_unique<TransportModel> (*edit);
        transportBar   = std::make_unique<TransportBarComponent> (*transportModel);
        arrangement    = std::make_unique<ArrangementView> (*edit, viewModel);

        transportBar->onPlayheadMoved = [this] (double secs, bool playing)
        {
            if (arrangement != nullptr)
                arrangement->setPlayheadSecs (secs, playing);
        };
        arrangement->onEdited = [this]
        { updateWindowTitle(); };

        addAndMakeVisible (*transportBar);
        addAndMakeVisible (*arrangement);
        resized();
        updateWindowTitle();
    }

    void newProject()
    {
        teardownUI();
        session->newProject (juce::File::createTempFile ("tracktionedit"), 4);
        buildUI();
    }

    void chooseOpen()
    {
        chooser = std::make_unique<juce::FileChooser> ("Open project", juce::File(), "*.tracktionedit");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f == juce::File())
                                      return;
                                  teardownUI();
                                  session->openProject (f);
                                  buildUI();
                              });
    }

    void chooseImport()
    {
        if (arrangement == nullptr)
            return;

        chooser = std::make_unique<juce::FileChooser> (
            "Import audio", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.caf");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (arrangement == nullptr)
                                      return;
                                  // Lay each selected file end-to-end from the visible start, onto
                                  // the first track — mirrors ArrangementView::filesDropped.
                                  double at = juce::jmax (0.0, viewModel.viewStartSecs());
                                  for (const auto& f : fc.getResults())
                                      if (auto* cc = arrangement->importFileAt (f, 0, at))
                                          at = cc->clip().getPosition().getEnd().inSeconds();
                              });
    }

    void doSave()
    {
        if (session->file().existsAsFile())
            session->save();
        else
            chooseSaveAs();
    }

    void chooseSaveAs()
    {
        chooser = std::make_unique<juce::FileChooser> ("Save project as", juce::File(), "*.tracktionedit");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == juce::File())
                                      return;
                                  if (! f.hasFileExtension ("tracktionedit"))
                                      f = f.withFileExtension ("tracktionedit");
                                  session->saveAs (f);
                                  updateWindowTitle();
                              });
    }

    void chooseExport()
    {
        chooser = std::make_unique<juce::FileChooser> ("Export WAV", juce::File(), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == juce::File() || session->edit() == nullptr)
                                      return;
                                  if (! f.hasFileExtension ("wav"))
                                      f = f.withFileExtension ("wav");
                                  exportEditToWav (*session->edit(), f);
                              });
    }

    void updateWindowTitle()
    {
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        {
            const auto name = session->file().getFileNameWithoutExtension();
            w->setName ("EZStudio - " + (name.isEmpty() ? juce::String ("Untitled") : name) + (session->hasUnsavedChanges() ? " *" : ""));
        }
    }

    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<ProjectSession> session;
    TimelineViewModel viewModel;
    std::unique_ptr<TransportModel> transportModel;
    std::unique_ptr<juce::MenuBarComponent> menuBar;
    std::unique_ptr<TransportBarComponent> transportBar;
    std::unique_ptr<ArrangementView> arrangement;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppComponent)
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
        setContentOwned (new AppComponent(), true);
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
