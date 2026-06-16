#pragma once

namespace tracktion
{
inline namespace engine
{
    class Edit;
} // namespace engine
} // namespace tracktion

namespace daw
{
/** Undo/redo facade over the Edit's juce::UndoManager. The UI binds its Undo/Redo
    commands to these; ClipOps already opens one transaction per gesture, so one undo()
    reverts one gesture. GUI-free (daw_core); takes a forward-declared te::Edit so
    callers need no tracktion include.

    Engine gotcha (paid for in the Chunk 3 null test): an Edit attaches its UndoManager
    asynchronously, so in a headless context (a test, or anything that mutates the Edit
    before the message loop has run) changes are NOT recorded for undo until the loop is
    pumped once. Call ensureUndoManagerReady() right after building/loading the Edit.
    A running GUI pumps its loop continuously, so it never needs this. */

/** Reverts the most recent gesture transaction. */
void undo (tracktion::engine::Edit&);

/** Re-applies the most recently undone gesture transaction. */
void redo (tracktion::engine::Edit&);

/** True if there is a gesture to undo / redo (for enabling menu items). */
bool canUndo (tracktion::engine::Edit&);
bool canRedo (tracktion::engine::Edit&);

/** Clears the undo history (e.g. just after building/loading so setup isn't undoable). */
void clearUndoHistory (tracktion::engine::Edit&);

/** Headless-only: pump the message loop briefly so the Edit attaches its UndoManager
    before any gesture is recorded. No-op-equivalent in a running GUI. */
void ensureUndoManagerReady (tracktion::engine::Edit&, int millisecondsToPump = 20);
} // namespace daw
