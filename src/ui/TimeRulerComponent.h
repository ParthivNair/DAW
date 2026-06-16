#pragma once

#include "engine/TimelineViewModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace tracktion
{
inline namespace engine
{
    class Edit;
} // namespace engine
} // namespace tracktion

namespace daw
{
/** The time ruler strip above the timeline: bar/beat gridlines and bar-number labels,
    laid out by the shared TimelineViewModel (so it always lines up with the clips) and the
    Edit's tempo sequence. Read-only painter. */
class TimeRulerComponent final : public juce::Component
{
public:
    TimeRulerComponent (tracktion::engine::Edit&, const TimelineViewModel&);

    void paint (juce::Graphics&) override;

private:
    tracktion::engine::Edit& edit;
    const TimelineViewModel& viewModel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeRulerComponent)
};
} // namespace daw
