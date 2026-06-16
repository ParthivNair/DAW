#pragma once

#include "RecentFilesList.h"

#include <juce_core/juce_core.h>
#include <memory>

namespace tracktion
{
inline namespace engine
{
    class Edit;
    class Engine;
} // namespace engine
} // namespace tracktion

namespace daw
{
/** Project lifecycle for the timeline MVP: new / open / save a `.tracktionedit`, plus
    dirty-state tracking and a recent-files list. GUI-free (daw_core); the UI's
    File menu drives this and reads back the current Edit + dirty flag.

    Owns the current Edit. Construct one per engine; call newProject or openProject to
    populate it. The engine outlives the session. */
class ProjectSession
{
public:
    explicit ProjectSession (tracktion::engine::Engine&);
    ~ProjectSession();

    /** Creates a fresh blank project bound to `file` (the file need not exist yet; it is
        written on the first save). The Edit starts with `numAudioTracks` audio tracks and
        neutral gains. Replaces any current project. */
    void newProject (const juce::File& file, int numAudioTracks = 1);

    /** Opens an existing `.tracktionedit`. Returns false (leaving the current project
        untouched) if the file does not exist or fails to load. Adds it to recents. */
    bool openProject (const juce::File& file);

    /** Saves to the current project file. Returns false on failure (or if no project is
        open). Clears the dirty flag and adds the file to recents. */
    bool save();

    /** Saves to `file`, which becomes the current project file. */
    bool saveAs (const juce::File& file);

    /** The current Edit, or nullptr if no project is open. */
    tracktion::engine::Edit* edit() const noexcept;

    /** The current project file (may not exist on disk yet for a new, unsaved project). */
    juce::File file() const;

    /** True if the project has unsaved changes since the last save. */
    bool hasUnsavedChanges() const;

    /** Most-recently-opened/saved project files (newest first), GUI-free. */
    const RecentFilesList& recentFiles() const noexcept { return recents; }

private:
    void bindEditToFile (const juce::File&);

    tracktion::engine::Engine& engine;
    std::unique_ptr<tracktion::engine::Edit> currentEdit;
    juce::File currentFile;
    RecentFilesList recents;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectSession)
};
} // namespace daw
