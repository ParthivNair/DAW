#pragma once

namespace daw
{
/** What an Edit is for. Maps to the engine's Edit::EditRole, but is declared here
    (instead of taking an Edit::EditRole) so headers that expose it stay free of the
    heavy tracktion include — it is pulled into the UI through the daw_core API.

    - livePlayback maps to Edit::forEditing, which creates an EditPlaybackContext so
      the transport can drive the output device. **Required for live sound** — the
      forRendering role sets playDisabled and produces no device output (the
      live-playback gotcha paid for in Phase 0; see dev/decisions.md).
    - offlineRender maps to Edit::forRendering (no playback context), used by the
      render tests, which render offline via RenderTask.

    The .cpp that builds the Edit does the EditPurpose -> Edit::EditRole mapping
    locally (a one-line ternary) so this header pulls in zero engine types. */
enum class EditPurpose
{
    livePlayback,
    offlineRender
};
} // namespace daw
