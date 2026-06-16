#pragma once

#include <juce_core/juce_core.h>

namespace tracktion
{
inline namespace engine
{
    class AudioTrack;
    class WaveAudioClip;
} // namespace engine
} // namespace tracktion

namespace daw
{
/** Imports an audio file (WAV/FLAC/MP3/etc. — anything the engine's
    AudioFileFormatManager can read) onto an audio track as a WaveAudioClip.

    GUI-free (daw_core): this is the import *logic* the timeline UI's drag-and-drop
    target calls, separated out so it can be render-tested without any GUI. The
    drag-and-drop wiring itself (FileDragAndDropTarget) lives in the UI.

    Validates the source with te::AudioFile::isValid() first and returns nullptr if
    the file can't be read (so a bad drop fails loudly rather than inserting a silent
    clip). The clip is placed at `startTime` on the timeline with its full natural
    length and zero source offset (it plays from the start of the file).

    @param track         destination audio track (e.g. daw::buildArrangementEdit's track 0).
    @param sourceFile    the audio file on disk. Must still exist at render/play time —
                         the engine reads from the path, not a copy.
    @param startTime     where the clip's left edge sits on the edit timeline.
    @param deleteExisting if true, clips already overlapping the new clip's range are
                          removed (engine's DeleteExistingClips behaviour); default false.
    @returns the new clip, or nullptr if the file was invalid. */
tracktion::engine::WaveAudioClip* importAudioFileAsClip (tracktion::engine::AudioTrack& track,
                                                         const juce::File& sourceFile,
                                                         double startTimeSecs,
                                                         bool deleteExisting = false);
} // namespace daw
