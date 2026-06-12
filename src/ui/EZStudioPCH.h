// Precompiled-header umbrella for the EZStudio GUI app target.
//
// Guarded on __cplusplus so any C-language PCH CMake also generates (the project
// enables C for JUCE's C TUs) is an empty no-op. EZStudio compiles only my own
// C++ UI TUs — all JUCE/tracktion module amalgamation sources live in the
// separate, PCH-free daw_engine target — so it is safe to force-include the
// heavy GUI + engine umbrellas here.
#pragma once

#ifdef __cplusplus
 #include <juce_gui_extra/juce_gui_extra.h>
 #include <tracktion_engine/tracktion_engine.h>
#endif
