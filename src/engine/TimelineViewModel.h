#pragma once

#include <juce_core/juce_core.h>

namespace tracktion
{
inline namespace engine
{
    class Edit;
} // namespace engine
} // namespace tracktion

namespace daw
{
/** Pure pixel <-> time mapping for the timeline view: the keystone that lets the UI
    component stay dumb (it asks this where a time sits in pixels and vice-versa) and lets
    zoom/scroll/snapping be unit-tested with no engine and no GUI.

    Holds the visible time window [viewStart, viewEnd] (seconds) and the component width
    (pixels). Engine-free and header-only — the musical conversions that DO need the
    Edit's tempo (snapping, bars:beats text) are free functions below. */
class TimelineViewModel
{
public:
    TimelineViewModel() = default;

    void setWidthPixels (int w) { widthPx = juce::jmax (1, w); }

    void setVisibleTimeRange (double startSecs, double endSecs)
    {
        viewStart = startSecs;
        // Keep a strictly-positive span so pixelsPerSecond is always finite.
        viewEnd = juce::jmax (startSecs + minSpanSecs, endSecs);
    }

    int widthPixels() const { return widthPx; }
    double viewStartSecs() const { return viewStart; }
    double viewEndSecs() const { return viewEnd; }
    double visibleSpanSecs() const { return viewEnd - viewStart; }
    double pixelsPerSecond() const { return widthPx / visibleSpanSecs(); }

    /** Pixel x (0 at the left of the view) for a time in seconds. */
    double timeToX (double secs) const { return (secs - viewStart) * pixelsPerSecond(); }

    /** Time in seconds for a pixel x. Inverse of timeToX (round-trips to < 1 px). */
    double xToTime (double x) const { return viewStart + x / pixelsPerSecond(); }

    /** Zooms by `factor` (>1 zooms in) keeping the time under pixel `anchorX` fixed. */
    void zoomBy (double factor, double anchorX)
    {
        factor                = juce::jlimit (1.0e-4, 1.0e4, factor);
        const double anchorT  = xToTime (anchorX);
        const double newSpan  = juce::jmax (minSpanSecs, visibleSpanSecs() / factor);
        const double fraction = juce::jlimit (0.0, 1.0, anchorX / (double) widthPx);
        const double newStart = anchorT - fraction * newSpan;
        setVisibleTimeRange (newStart, newStart + newSpan);
    }

    /** Scrolls the view by a pixel delta (positive scrolls content left). */
    void scrollByPixels (double dx)
    {
        const double dt = dx / pixelsPerSecond();
        setVisibleTimeRange (viewStart + dt, viewEnd + dt);
    }

    /** Auto-follow: if `secs` is left of the view or past the right margin, shift the view
        (keeping its span) so the playhead sits `marginFraction` in from the left. */
    void followPlayhead (double secs, double marginFraction = 0.1)
    {
        const double span   = visibleSpanSecs();
        const double margin = juce::jlimit (0.0, 0.45, marginFraction) * span;
        if (secs < viewStart || secs > viewEnd - margin)
        {
            const double newStart = secs - margin;
            setVisibleTimeRange (newStart, newStart + span);
        }
    }

private:
    static constexpr double minSpanSecs = 1.0e-3;

    double viewStart = 0.0;
    double viewEnd   = 10.0; // a sane default window until the UI sets one
    int widthPx      = 1000;
};

//==============================================================================
// Musical time helpers — need the Edit's TempoSequence, so they live in the .cpp.

/** Snaps a time (seconds) to the nearest bar line. */
double snapSecondsToBar (tracktion::engine::Edit&, double secs);

/** Snaps a time (seconds) to the nearest beat. */
double snapSecondsToBeat (tracktion::engine::Edit&, double secs);

/** Bars|beats readout for a time, 1-based (e.g. "2|3" = bar 2, beat 3). */
juce::String barsBeatsText (tracktion::engine::Edit&, double secs);

/** min:sec.ms readout for a time (e.g. "1:02.500"). Engine-free but kept here with its
    sibling so the transport readout has one home. */
juce::String minSecText (double secs);
} // namespace daw
