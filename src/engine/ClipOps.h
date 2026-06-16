#pragma once

namespace tracktion
{
inline namespace engine
{
    class Clip;
    class AudioClipBase;
} // namespace engine
} // namespace tracktion

namespace daw
{
/** Fade curve shape, mirroring te::AudioFadeCurve::Type (1=linear … 4=sCurve) but
    declared here so this header stays free of the engine include — the UI picks a
    curve from this enum and the .cpp maps it. */
enum class FadeCurve
{
    linear,
    convex,
    concave,
    sCurve
};

/** GUI-free clip-editing operations on a single clip — the engine half of the Phase 1
    timeline's clip interactions (drag-move, trim handles, fade handles, gain, delete).
    They exist and are render-tested before any UI drag handle is built.

    Each op opens its own UndoManager transaction (edit.getUndoManager().
    beginNewTransaction) so one user gesture == one undo step; the engine's
    ValueTree-backed mutations are recorded into that transaction automatically. The
    undo/redo round-trip is proven by the Chunk 3 null test.

    Position ops take a te::Clip (the base every clip type shares); audio ops take a
    te::AudioClipBase (gain/fades are an audio-clip concept). A WaveAudioClip is both.
    All times are in seconds. */

/** Moves the clip so its left edge sits at `newStartSecs`, keeping its length and the
    audio it plays (the source moves with the clip). */
void moveClip (tracktion::engine::Clip&, double newStartSecs);

/** Sets the clip's length (trim the right edge), keeping its start and source offset. */
void setClipLength (tracktion::engine::Clip&, double newLengthSecs);

/** Trims the left edge to `newStartSecs`: moves the start while keeping the source aligned
    to the timeline (so the clip reveals less/more of its beginning), shortening/lengthening
    the clip — i.e. setStart(newStart, preserveSync=true, keepLength=false). */
void trimClipLeftTo (tracktion::engine::Clip&, double newStartSecs);

/** Sets the clip's source offset — which part of the file plays at the clip's start. */
void setClipOffset (tracktion::engine::Clip&, double newOffsetSecs);

/** Removes the clip from its track (delete). The clip pointer is invalid afterwards. */
void deleteClip (tracktion::engine::Clip&);

/** Sets the clip's gain in dB. */
void setClipGainDb (tracktion::engine::AudioClipBase&, float gainDb);

/** Sets a fade-in of `lengthSecs` with the given curve. The length is clamped to the
    clip and shrunk to avoid overrunning the fade-out. (We return void deliberately:
    the engine's AudioClipBase::setFadeIn returns bool but always returns false even on
    success — an upstream bug — so verify via getFadeIn() / a render, not the return.) */
void setClipFadeIn (tracktion::engine::AudioClipBase&, double lengthSecs, FadeCurve = FadeCurve::convex);

/** Sets a fade-out of `lengthSecs` with the given curve. See setClipFadeIn re: void. */
void setClipFadeOut (tracktion::engine::AudioClipBase&, double lengthSecs, FadeCurve = FadeCurve::concave);

/** Applies the engine's short anti-click edge fades (~30 ms in/out). */
void applyClipEdgeFades (tracktion::engine::AudioClipBase&);
} // namespace daw
