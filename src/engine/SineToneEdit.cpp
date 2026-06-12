#include "SineToneEdit.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace daw
{
std::unique_ptr<te::Edit> buildSineToneEdit (te::Engine& engine, const SineToneSpec& spec)
{
    // Single-track Edit with unity master volume (matches the engine's own test
    // edit). 60 bpm tempo is irrelevant here — the tone source ignores tempo —
    // but we leave the engine defaults and just neutralise gains so the only
    // amplitude in the chain is the Tone Generator's level.
    auto edit = te::Edit::createSingleTrackEdit (engine, te::Edit::EditRole::forRendering);
    edit->getMasterVolumePlugin()->setVolumeDb (0.0f);

    auto track = te::getAudioTracks (*edit)[0];
    jassert (track != nullptr);
    track->getVolumePlugin()->setVolumeDb (0.0f);

    // The Tone Generator is NOT one of the engine's default built-in plugin
    // types (PluginManager::initialise() omits it), so register it before asking
    // the cache to create one. registerBuiltInType is idempotent — it skips a
    // type that's already present — so calling this per-edit is safe.
    engine.getPluginManager().createBuiltInType<te::ToneGeneratorPlugin>();

    // Create + insert the Tone Generator at the head of the track's plugin list.
    auto plugin = edit->getPluginCache().createNewPlugin (te::ToneGeneratorPlugin::xmlTypeName, {});
    auto* tone = dynamic_cast<te::ToneGeneratorPlugin*> (plugin.get());
    jassert (tone != nullptr);

    // Write the CachedValues (persisted in the plugin state) ...
    tone->oscType   = static_cast<float> (te::ToneGeneratorPlugin::OscType::sin);
    tone->frequency = static_cast<float> (spec.frequencyHz);
    tone->level     = spec.levelLinear;

    // ... then push them into the AutomatableParameters the DSP actually reads in
    // applyToBuffer() (frequencyParam/levelParam->getCurrentValue()). Setting the
    // CachedValue alone leaves the parameter's cached currentValue stale, which is
    // why we mirror restorePluginStateFromValueTree's updateFromAttachedValue().
    for (auto p : tone->getAutomatableParameters())
        p->updateFromAttachedValue();

    track->pluginList.insertPlugin (plugin, 0, nullptr);

    // The Tone Generator is a synth with no clips, so the Edit has no content
    // length of its own. Callers bound playback/render to spec.durationSecs
    // explicitly (render Parameters::time, or the transport loop range).
    return edit;
}
} // namespace daw
