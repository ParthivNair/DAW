#pragma once

#include <juce_core/juce_core.h>

namespace daw
{
/** Returns a human-readable string describing the engine/JUCE versions this
    build of daw_core was compiled against. GUI-free; safe to call from tests. */
juce::String engineInfoString();

/** True when this build compiled the Rubber Band time-stretcher in
    (TRACKTION_BUILD_RUBBERBAND). SoundTouch is always available as the fallback. */
bool rubberBandBuiltIn() noexcept;
} // namespace daw
