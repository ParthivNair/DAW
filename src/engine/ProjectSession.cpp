#include "ProjectSession.h"

#include "ArrangementEdit.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
ProjectSession::ProjectSession (te::Engine& e)
    : engine (e)
{
    recents.setMaxItems (16);
}

ProjectSession::~ProjectSession() = default;

void ProjectSession::bindEditToFile (const juce::File& f)
{
    currentFile = f;
    // editFileRetriever lets the Edit resolve its own file (and relative audio-file
    // paths) without a ProjectManager — the demo pattern for file-based edits.
    if (currentEdit != nullptr)
        currentEdit->editFileRetriever = [f]
        { return f; };
}

void ProjectSession::newProject (const juce::File& f, int numAudioTracks)
{
    currentEdit = te::createEmptyEdit (engine, f);
    configureArrangementTracks (*currentEdit, numAudioTracks);
    bindEditToFile (f);

    // A brand-new project has no on-disk file yet, so it counts as unsaved.
    currentEdit->markAsChanged();
}

bool ProjectSession::openProject (const juce::File& f)
{
    if (! f.existsAsFile())
        return false;

    auto loaded = te::loadEditFromFile (engine, f);
    if (loaded == nullptr)
        return false;

    currentEdit = std::move (loaded);
    bindEditToFile (f);
    currentEdit->resetChangedStatus();

    recents.add (f);
    return true;
}

bool ProjectSession::save()
{
    if (currentEdit == nullptr)
        return false;

    // Push any CachedValue/pending changes into the state tree before serialising, so a
    // round-trip preserves clip gain/fades etc. (save() does not guarantee this for us).
    currentEdit->flushState();

    // save(warnOfFailure, forceSaveEvenIfNotModified, offerToDiscardChanges): force a
    // write even when the in-memory dirty flag says clean, and never pop a dialog.
    const bool ok = te::EditFileOperations (*currentEdit).save (true, true, false);

    if (ok)
        recents.add (currentFile);

    return ok;
}

bool ProjectSession::saveAs (const juce::File& f)
{
    if (currentEdit == nullptr)
        return false;

    bindEditToFile (f);
    return save();
}

te::Edit* ProjectSession::edit() const noexcept
{
    return currentEdit.get();
}

juce::File ProjectSession::file() const
{
    return currentFile;
}

bool ProjectSession::hasUnsavedChanges() const
{
    return currentEdit != nullptr && currentEdit->hasChangedSinceSaved();
}
} // namespace daw
