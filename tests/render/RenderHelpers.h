#pragma once

// Reusable building blocks for render-to-WAV tests. Keep these tiny and
// dependency-light: a render test should read top-to-bottom as
//   build edit -> renderEditToWav -> readWavToBuffer -> assert.
// Future render tests (null tests, golden compares) reuse these same helpers.

#include <juce_audio_formats/juce_audio_formats.h>
#include <tracktion_engine/tracktion_engine.h>

#include <cmath>
#include <memory>
#include <optional>

namespace daw::render_test
{
namespace te = tracktion::engine;

/** Renders an Edit to a temp WAV synchronously, returning the file via a
    TemporaryFile that deletes itself when it goes out of scope.

    We drive a Renderer::RenderTask inline (pumping runJob() to completion)
    instead of calling Renderer::renderToFile(taskDescription, params): that
    overload routes through UIBehaviour::runTaskWithProgressBar, which never
    returns under a headless engine with no message loop. The inline pump is
    exactly what Renderer's own useThread=false path does, so it runs on the
    calling thread, finishes in well under a second, and works cleanly under
    RTSan. */
inline std::shared_ptr<juce::TemporaryFile>
renderEditToWav (te::Edit& edit, double lengthSecs, double sampleRate, int blockSize = 512)
{
    auto temp = std::make_shared<juce::TemporaryFile> (".wav");

    te::Renderer::Parameters params (edit);
    params.destFile           = temp->getFile();
    params.audioFormat        = edit.engine.getAudioFileFormatManager().getWavFormat();
    params.sampleRateForAudio = sampleRate;
    params.blockSizeForAudio  = blockSize;
    params.bitDepth           = 24;
    params.time               = { tracktion::TimePosition(),
                                  tracktion::TimePosition::fromSeconds (lengthSecs) };
    params.canRenderInMono    = true;
    params.usePlugins         = true;
    // Render every track in the edit (the lone tone track).
    params.tracksToDo = te::toBitSet (te::getAllTracks (edit));
    // A synth tone always produces audio, but be explicit so a silent edit fails
    // the render rather than silently writing zeros.
    params.checkNodesForAudio = true;

    te::Renderer::RenderTask task ("render_test", params, nullptr, nullptr);

    // Pump the render to completion. Between blocks we give the message thread a brief
    // slice so the engine's AudioFileManager/cache can service a wave clip's async file
    // reads. A tone/synth source needs no file I/O, so Phase 0's tight loop sufficed;
    // a clip that reads audio off disk would otherwise starve those async operations
    // and the render crawls (~20x slower). This mirrors the engine's own
    // renderToAudioBufferDispatchingMessageThread. JUCE_MODAL_LOOPS_PERMITTED=1 (set in
    // CMake) is what allows runDispatchLoopUntil here, on the test's message thread.
    auto* mm = juce::MessageManager::getInstanceWithoutCreating();
    while (task.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
    {
        if (mm != nullptr)
            mm->runDispatchLoopUntil (1);
    }

    jassert (task.errorMessage.isEmpty());
    jassert (temp->getFile().existsAsFile());

    return temp;
}

/** Reads an entire WAV into a float AudioBuffer. Returns nullopt if the file
    can't be read. */
inline std::optional<juce::AudioBuffer<float>>
readWavToBuffer (const juce::File& wav, double& sampleRateOut)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (wav));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return std::nullopt;

    sampleRateOut = reader->sampleRate;

    juce::AudioBuffer<float> buffer ((int) reader->numChannels,
                                     (int) reader->lengthInSamples);
    reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
    return buffer;
}

/** RMS of one channel, expressed in dBFS (full-scale = sine of amplitude 1.0).
    Skips the first/last `edgeSkip` samples so transport ramp-in/out doesn't
    pollute the steady-state level. */
inline float rmsDbfs (const juce::AudioBuffer<float>& buffer, int channel, int edgeSkip)
{
    const int n     = buffer.getNumSamples();
    const int start = juce::jmin (edgeSkip, n);
    const int len   = juce::jmax (0, n - 2 * edgeSkip);
    if (len <= 0)
        return -std::numeric_limits<float>::infinity();

    const float rms = buffer.getRMSLevel (channel, start, len);
    return juce::Decibels::gainToDecibels (rms, -200.0f);
}

/** RMS of one channel over an explicit sample range, expressed in dBFS. Clamps the
    range to the buffer. Returns -inf for an empty range. Used to assert that a region
    is silent (e.g. before a clip's start) or to measure a clip body's steady-state
    level when the signal does not fill the whole render. */
inline float regionRmsDbfs (const juce::AudioBuffer<float>& buffer, int channel,
                            int startSample, int numSamples)
{
    const int n = buffer.getNumSamples();
    startSample = juce::jlimit (0, n, startSample);
    numSamples  = juce::jlimit (0, n - startSample, numSamples);
    if (numSamples <= 0)
        return -std::numeric_limits<float>::infinity();

    const float rms = buffer.getRMSLevel (channel, startSample, numSamples);
    return juce::Decibels::gainToDecibels (rms, -200.0f);
}

/** A non-owning AudioBuffer view over [startSample, startSample+numSamples) of
    `buffer`, so the existing whole-buffer helpers (rmsDbfs, dominantFrequency) can be
    pointed at a sub-range — e.g. the body of a clip that sits partway through a
    render. Shares samples with the parent; valid only while the parent lives. */
inline juce::AudioBuffer<float>
regionView (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const int n = buffer.getNumSamples();
    startSample = juce::jlimit (0, n, startSample);
    numSamples  = juce::jlimit (0, n - startSample, numSamples);
    return juce::AudioBuffer<float> (buffer.getArrayOfWritePointers(),
                                     buffer.getNumChannels(), startSample, numSamples);
}

/** Peak absolute per-sample difference between two equally-sized buffers, in dBFS.
    The workhorse of a null test: render path A and path B, subtract, and assert the
    residual is essentially silence (peak diff < -96 dBFS). Returns -inf for a perfect
    null. If the buffers differ in size/channels, compares the overlapping region and
    counts the size mismatch as full-scale (returns 0 dBFS) so the test fails loudly. */
inline float peakDiffDbfs (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
        return 0.0f; // full-scale: shapes differ, not a null

    float peak = 0.0f;
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
    {
        const float* pa = a.getReadPointer (ch);
        const float* pb = b.getReadPointer (ch);
        for (int i = 0; i < a.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (pa[i] - pb[i]));
    }
    return juce::Decibels::gainToDecibels (peak, -200.0f);
}

/** True if any sample in the buffer is NaN or Inf. */
inline bool hasNonFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* d = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (d[i]))
                return true;
    }
    return false;
}

/** Result of a single-bin-resolution FFT peak search. */
struct PeakBin
{
    int binIndex           = 0;
    double frequencyHz     = 0.0;
    double binResolutionHz = 0.0; ///< sampleRate / fftSize — the +/- tolerance.
};

/** Finds the dominant frequency of one channel via a windowed FFT.

    Copies `fftSize` samples from the steady-state middle of the buffer, applies a
    Hann window (to keep a non-bin-centred tone from smearing across bins), runs a
    real FFT, and returns the magnitude-peak bin (skipping DC). */
inline PeakBin dominantFrequency (const juce::AudioBuffer<float>& buffer, int channel,
                                  double sampleRate, int fftOrder = 14)
{
    const int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft (fftOrder);
    juce::dsp::WindowingFunction<float> window ((size_t) fftSize,
                                                juce::dsp::WindowingFunction<float>::hann);

    std::vector<float> data ((size_t) fftSize * 2, 0.0f);

    // Take the window from the centre of the signal to avoid edge ramps.
    const int avail  = buffer.getNumSamples();
    const int start  = juce::jmax (0, (avail - fftSize) / 2);
    const int n      = juce::jmin (fftSize, avail - start);
    const float* src = buffer.getReadPointer (channel);
    for (int i = 0; i < n; ++i)
        data[(size_t) i] = src[start + i];

    window.multiplyWithWindowingTable (data.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (data.data());

    int peakBin   = 1;
    float peakMag = 0.0f;
    for (int bin = 1; bin < fftSize / 2; ++bin) // skip DC at bin 0
    {
        if (data[(size_t) bin] > peakMag)
        {
            peakMag = data[(size_t) bin];
            peakBin = bin;
        }
    }

    const double binResolution = sampleRate / fftSize;
    return { peakBin, peakBin * binResolution, binResolution };
}
} // namespace daw::render_test
