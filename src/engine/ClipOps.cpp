#include "ClipOps.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
namespace
{
    te::AudioFadeCurve::Type toEngineCurve (FadeCurve c)
    {
        switch (c)
        {
            case FadeCurve::linear:
                return te::AudioFadeCurve::linear;
            case FadeCurve::convex:
                return te::AudioFadeCurve::convex;
            case FadeCurve::concave:
                return te::AudioFadeCurve::concave;
            case FadeCurve::sCurve:
                return te::AudioFadeCurve::sCurve;
        }
        return te::AudioFadeCurve::linear;
    }

    /** Opens a fresh undo transaction for one gesture so it collapses to a single
        undo step. Engine mutations after this are recorded into it automatically. */
    void beginGesture (te::Clip& clip, const char* name)
    {
        clip.edit.getUndoManager().beginNewTransaction (name);
    }
} // namespace

void moveClip (te::Clip& clip, double newStartSecs)
{
    beginGesture (clip, "Move clip");
    // preserveSync=false: the source moves with the clip, so the same audio plays from
    // the clip's start. keepLength=true: the end follows, preserving the clip's length.
    clip.setStart (tracktion::TimePosition::fromSeconds (newStartSecs),
                   /*preserveSync*/ false, /*keepLength*/ true);
}

void setClipLength (te::Clip& clip, double newLengthSecs)
{
    beginGesture (clip, "Trim clip");
    clip.setLength (tracktion::TimeDuration::fromSeconds (newLengthSecs), /*preserveSync*/ false);
}

void setClipOffset (te::Clip& clip, double newOffsetSecs)
{
    beginGesture (clip, "Set clip offset");
    clip.setOffset (tracktion::TimeDuration::fromSeconds (newOffsetSecs));
}

void deleteClip (te::Clip& clip)
{
    beginGesture (clip, "Delete clip");
    clip.removeFromParent();
}

void setClipGainDb (te::AudioClipBase& clip, float gainDb)
{
    beginGesture (clip, "Set clip gain");
    clip.setGainDB (gainDb);
}

void setClipFadeIn (te::AudioClipBase& clip, double lengthSecs, FadeCurve curve)
{
    beginGesture (clip, "Set fade in");
    clip.setFadeInType (toEngineCurve (curve));
    // Return value ignored: setFadeIn always returns false even on success (upstream bug).
    clip.setFadeIn (tracktion::TimeDuration::fromSeconds (lengthSecs));
}

void setClipFadeOut (te::AudioClipBase& clip, double lengthSecs, FadeCurve curve)
{
    beginGesture (clip, "Set fade out");
    clip.setFadeOutType (toEngineCurve (curve));
    clip.setFadeOut (tracktion::TimeDuration::fromSeconds (lengthSecs));
}

void applyClipEdgeFades (te::AudioClipBase& clip)
{
    beginGesture (clip, "Apply edge fades");
    clip.applyEdgeFades();
}
} // namespace daw
