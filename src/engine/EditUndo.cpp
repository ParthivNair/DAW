#include "EditUndo.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
void undo (te::Edit& edit)
{
    edit.getUndoManager().undo();
}

void redo (te::Edit& edit)
{
    edit.getUndoManager().redo();
}

bool canUndo (te::Edit& edit)
{
    return edit.getUndoManager().canUndo();
}

bool canRedo (te::Edit& edit)
{
    return edit.getUndoManager().canRedo();
}

void clearUndoHistory (te::Edit& edit)
{
    edit.getUndoManager().clearUndoHistory();
}

void ensureUndoManagerReady (te::Edit& edit, int millisecondsToPump)
{
    juce::ignoreUnused (edit);

    // The Edit attaches its UndoManager via an async message; pump the loop so that
    // happens before any gesture is recorded. Mirrors the engine's own MidiList test
    // ("pump the dispatch loop here to ensure the Edit has attached the undo manager").
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
        mm->runDispatchLoopUntil (millisecondsToPump);
}
} // namespace daw
