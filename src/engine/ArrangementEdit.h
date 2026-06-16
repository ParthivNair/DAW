#pragma once

#include "EditPurpose.h"

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
/** Builds a fresh, empty multi-track Edit for the timeline/arrangement MVP.

    GUI-free (compiles into daw_core, runs headless under daw_tests). This is the
    Phase 1 analog of buildSineToneEdit: the single place an arrangement Edit is
    constructed, so the render tests and the GUI build *the same* thing.

    The Edit starts with `numAudioTracks` audio tracks (each with the engine's
    default Volume + LevelMeter plugins), master and per-track volume neutralised to
    unity (0 dB) so that the only gains in the rendered signal are the ones a clip or
    a deliberate edit introduces — the same "neutralise the chain" discipline the
    sine render test relies on for exact level assertions.

    No clips are added; callers (ClipImporter, the UI) populate tracks afterwards.

    @param numAudioTracks  how many audio tracks to pre-allocate (>= 1).
    @param purpose         livePlayback (forEditing, device output) or offlineRender
                           (forRendering, used by the render tests). */
std::unique_ptr<tracktion::engine::Edit> buildArrangementEdit (tracktion::engine::Engine&,
                                                               int numAudioTracks = 1,
                                                               EditPurpose        = EditPurpose::livePlayback);

/** Brings an existing Edit up to `numAudioTracks` audio tracks (ensureNumberOfAudioTracks)
    and neutralises master + per-track volume to unity. Shared by buildArrangementEdit and
    ProjectSession::newProject (which start from createSingleTrackEdit / createEmptyEdit
    respectively) so the "blank arrangement" shape is defined in one place. */
void configureArrangementTracks (tracktion::engine::Edit&, int numAudioTracks);
} // namespace daw
