#pragma once

#include "SineToneEdit.h"

#include <juce_core/juce_core.h>
#include <memory>

namespace tracktion { inline namespace engine { class Edit; class Engine; } }

namespace daw
{
/** The "first sound" milestone, GUI-free.

    Owns a headless tracktion Engine + an Edit holding a 440 Hz sine (see
    SineToneSpec), opens the default CoreAudio output device, and plays the tone on
    a loop. Lives in daw_core so it carries ZERO GUI includes; the JUCE app shell
    (src/ui) just constructs one of these and calls play().

    Real-time note: none of this class runs on the audio thread. It only
    configures the engine on the message thread; the engine's own graph does the
    RT work. So nothing here is RT_NONBLOCKING — there is no OUR-code path reachable
    from the audio callback in this milestone. */
class SineDemoSession
{
public:
    /** Constructs the engine + edit and initialises the default audio device.
        Must be called on the message thread (the engine assumes it). */
    explicit SineDemoSession (const SineToneSpec& = {});
    ~SineDemoSession();

    /** Starts looping playback of the tone through the default output device. */
    void play();
    /** Stops playback. */
    void stop();
    /** True while the transport is playing. */
    bool isPlaying() const;

    /** Human-readable summary of the open output device (name, sample rate), or a
        note if no device opened. For the GUI/console to log at launch. */
    juce::String outputDeviceDescription() const;

private:
    SineToneSpec spec;
    std::unique_ptr<tracktion::engine::Engine> engine;
    std::unique_ptr<tracktion::engine::Edit>   edit;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SineDemoSession)
};
} // namespace daw
