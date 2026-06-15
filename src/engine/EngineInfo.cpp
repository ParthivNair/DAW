#include "EngineInfo.h"

#include <tracktion_engine/tracktion_engine.h>

namespace daw
{
juce::String engineInfoString()
{
    return juce::String ("EZStudio daw_core | JUCE ") + JUCE_STRINGIFY (JUCE_MAJOR_VERSION) + "." + JUCE_STRINGIFY (JUCE_MINOR_VERSION) + "." + JUCE_STRINGIFY (JUCE_BUILDNUMBER) + " | Rubber Band " + (rubberBandBuiltIn() ? "on" : "off") + " | SoundTouch on";
}

bool rubberBandBuiltIn() noexcept
{
#if TRACKTION_BUILD_RUBBERBAND
    return true;
#else
    return false;
#endif
}
} // namespace daw
