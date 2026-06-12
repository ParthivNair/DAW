// Process-wide JUCE lifetime for the test runner.
//
// Any test that constructs a tracktion Engine spins up JUCE's global singletons
// (MessageManager, ShutdownDetector, async updaters, ...). Without a
// ScopedJuceInitialiser_GUI alive for the whole process those singletons are
// still standing when JUCE's leak detector runs at static-destruction time, so
// it reports them as "leaked". Holding one initialiser for the entire Catch2 run
// (created before the first test, destroyed after the last) lets JUCE shut its
// singletons down in order, which clears the false-positive leak reports.
//
// We do NOT define Catch2's main here — Catch2WithMain still owns main(). This
// is just a global listener that brackets the run.

#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <juce_events/juce_events.h>

#include <memory>

namespace
{
class JuceLifetimeListener final : public Catch::EventListenerBase
{
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting (const Catch::TestRunInfo&) override
    {
        juceInit = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
    }

    void testRunEnded (const Catch::TestRunStats&) override
    {
        juceInit.reset();
    }

private:
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> juceInit;
};
} // namespace

CATCH_REGISTER_LISTENER (JuceLifetimeListener)
