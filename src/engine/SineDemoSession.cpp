#include "SineDemoSession.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;
using namespace tracktion::literals;

namespace daw
{
SineDemoSession::SineDemoSession (const SineToneSpec& s)
    : spec (s)
{
    // Headless-by-default Engine: the simple ctor uses the engine's default
    // PropertyStorage / UIBehaviour. No GUI types are referenced.
    engine = std::make_unique<te::Engine> ("EZStudio");

    // Open the default CoreAudio output (and inputs) so the transport can play
    // through the speakers. This is the only difference from the render test,
    // which never touches the device manager.
    engine->getDeviceManager().initialise();

    edit = buildSineToneEdit (*engine, spec);
}

SineDemoSession::~SineDemoSession()
{
    if (edit != nullptr)
        edit->getTransport().stop (false, false);

    edit.reset();

    if (engine != nullptr)
        engine->getDeviceManager().closeDevices();

    engine.reset();
}

void SineDemoSession::play()
{
    if (edit == nullptr)
        return;

    auto& transport = edit->getTransport();

    // Loop the fixed-length tone region forever so the first-sound demo keeps
    // playing without an explicit end.
    transport.setLoopRange ({ 0_tp, tracktion::TimePosition::fromSeconds (spec.durationSecs) });
    transport.looping = true;
    transport.ensureContextAllocated();
    transport.playFromStart (true);
}

void SineDemoSession::stop()
{
    if (edit != nullptr)
        edit->getTransport().stop (false, false);
}

bool SineDemoSession::isPlaying() const
{
    return edit != nullptr && edit->getTransport().isPlaying();
}

juce::String SineDemoSession::outputDeviceDescription() const
{
    if (engine == nullptr)
        return "no engine";

    auto& dm = engine->getDeviceManager();

    if (auto* out = dm.getDefaultWaveOutDevice())
        return out->getName()
             + " @ " + juce::String (dm.getSampleRate(), 0) + " Hz"
             + ", block " + juce::String (dm.getBlockSize());

    return "no default output device opened";
}
} // namespace daw
