#include "ArrangementRenderer.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
ExportResult exportEditToWav (te::Edit& edit, const juce::File& destination, const ExportOptions& opts)
{
    ExportResult result;

    destination.getParentDirectory().createDirectory();
    destination.deleteFile();

    const auto start = tracktion::TimePosition::fromSeconds (opts.startSecs);
    const auto end   = opts.endSecs < 0.0
                           ? tracktion::TimePosition() + edit.getLength()
                           : tracktion::TimePosition::fromSeconds (opts.endSecs);

    te::Renderer::Parameters params (edit);
    params.destFile           = destination;
    params.audioFormat        = edit.engine.getAudioFileFormatManager().getWavFormat();
    params.sampleRateForAudio = opts.sampleRate;
    params.blockSizeForAudio  = opts.blockSize;
    params.bitDepth           = opts.bitDepth;
    params.time               = { start, end };
    params.usePlugins         = true;
    params.useMasterPlugins   = opts.useMasterPlugins;
    params.canRenderInMono    = true;
    params.checkNodesForAudio = opts.failIfSilent;
    params.tracksToDo         = te::toBitSet (te::getAllTracks (edit));

    te::Renderer::RenderTask task ("Export", params, nullptr, nullptr);

    // Inline pump + message-loop slice between blocks (so wave clips' async file reads
    // are serviced). See RenderHelpers.h / decisions.md Chunk 1.
    auto* mm = juce::MessageManager::getInstanceWithoutCreating();
    while (task.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
    {
        if (mm != nullptr)
            mm->runDispatchLoopUntil (1);
    }

    result.errorMessage  = task.errorMessage;
    result.success       = task.errorMessage.isEmpty() && destination.existsAsFile();
    result.lengthSeconds = (end - start).inSeconds();
    return result;
}
} // namespace daw
