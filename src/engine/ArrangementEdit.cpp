#include "ArrangementEdit.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
std::unique_ptr<te::Edit> buildArrangementEdit (te::Engine& engine, int numAudioTracks, EditPurpose purpose)
{
    // livePlayback -> forEditing creates an EditPlaybackContext so the transport can
    // drive the output device; forRendering sets playDisabled (no device output) and
    // is only for the offline render path (RenderTask). Same mapping as buildSineToneEdit.
    const auto role = purpose == EditPurpose::livePlayback ? te::Edit::EditRole::forEditing
                                                           : te::Edit::EditRole::forRendering;

    // createSingleTrackEdit gives one audio track + a master volume plugin already
    // wired; configureArrangementTracks tops it up to the requested count and
    // neutralises the gains.
    auto edit = te::Edit::createSingleTrackEdit (engine, role);
    configureArrangementTracks (*edit, numAudioTracks);
    return edit;
}

void configureArrangementTracks (te::Edit& edit, int numAudioTracks)
{
    jassert (numAudioTracks >= 1);
    numAudioTracks = juce::jmax (1, numAudioTracks);

    // ensureNumberOfAudioTracks is idempotent; each new track gets the default
    // Volume + LevelMeter plugins.
    edit.ensureNumberOfAudioTracks (numAudioTracks);

    // Neutralise every gain in the chain so a render reflects only clip-level edits.
    edit.getMasterVolumePlugin()->setVolumeDb (0.0f);

    for (auto track : te::getAudioTracks (edit))
        if (auto* vol = track->getVolumePlugin())
            vol->setVolumeDb (0.0f);
}
} // namespace daw
