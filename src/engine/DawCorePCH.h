// Precompiled-header umbrella for the my-code targets (daw_core, EZStudio).
//
// Guarded on __cplusplus so that any C-language PCH CMake generates for the
// target (the project enables C for JUCE's C TUs) is an empty no-op — the JUCE
// and tracktion umbrella headers are C++-only. These targets compile only my own
// C++ TUs, so it is safe to force-include the heavy umbrellas here; the JUCE
// module amalgamation files live in the separate, PCH-free daw_engine target.
#pragma once

#ifdef __cplusplus
 #include <juce_core/juce_core.h>
 #include <tracktion_engine/tracktion_engine.h>
#endif
