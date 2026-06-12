// Anchor translation unit for the daw_engine static library.
//
// daw_engine exists to amalgamate the tracktion_engine + JUCE module sources
// (and the single-file Rubber Band build) into one library, exactly once, with
// NO precompiled header (the JUCE module amalgamation TUs reject any
// force-included prefix header). A STATIC library needs at least one of its own
// object files, so this file provides one trivial symbol.

namespace daw::detail
{
// Defined so the archive is never empty; never called.
const char* daw_engine_build_anchor() noexcept { return "EZStudio daw_engine"; }
} // namespace daw::detail
