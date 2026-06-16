#include "ClipImporter.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
te::WaveAudioClip* importAudioFileAsClip (te::AudioTrack& track,
                                          const juce::File& sourceFile,
                                          double startTimeSecs,
                                          bool deleteExisting)
{
    auto& engine = track.edit.engine;

    // Validate before inserting: an invalid/unreadable file would otherwise insert a
    // clip that renders silence. getLength() is only meaningful once isValid() passes.
    te::AudioFile audioFile (engine, sourceFile);
    if (! audioFile.isValid())
        return nullptr;

    const auto start  = tracktion::TimePosition::fromSeconds (startTimeSecs);
    const auto length = tracktion::TimeDuration::fromSeconds (audioFile.getLength());

    // ClipPosition = { time range on the timeline, source offset }. Offset {} means
    // the clip plays from sample 0 of the file. insertWaveClip takes the juce::File
    // directly (not a ProjectItemID) — the form the engine demos use.
    auto clip = track.insertWaveClip (sourceFile.getFileNameWithoutExtension(),
                                      sourceFile,
                                      te::ClipPosition { { start, length }, {} },
                                      deleteExisting);

    return clip.get();
}
} // namespace daw
