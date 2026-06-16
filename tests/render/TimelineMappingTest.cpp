// Phase 1, Chunk 6 — the GUI-free presentation model: pixel<->time mapping, zoom/scroll,
// auto-follow, bars:beats / min:sec readouts, and snapping. The pure-math parts need no
// engine; the musical parts use a 120 bpm / 4-4 edit, and one render ties snap-to-bar back
// to audible truth (a clip snapped to a bar starts exactly at that bar's time).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/ArrangementEdit.h"
#include "engine/ClipImporter.h"
#include "engine/ClipOps.h"
#include "engine/SineToneEdit.h"
#include "engine/TimelineViewModel.h"
#include "render/RenderHelpers.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;
using Catch::Matchers::WithinAbs;

TEST_CASE ("TimelineViewModel maps pixels<->time and zooms/scrolls/follows", "[timeline]")
{
    daw::TimelineViewModel vm;
    vm.setWidthPixels (1000);
    vm.setVisibleTimeRange (0.0, 10.0); // 100 px/s

    CHECK (vm.pixelsPerSecond() == Catch::Approx (100.0));
    CHECK (vm.timeToX (0.0) == Catch::Approx (0.0));
    CHECK (vm.timeToX (10.0) == Catch::Approx (1000.0));
    CHECK (vm.timeToX (2.5) == Catch::Approx (250.0));

    // Round-trip xToTime(timeToX(t)) == t to well under a pixel.
    for (double t : { 0.0, 1.0, 3.33, 7.5, 9.99 })
        CHECK (vm.xToTime (vm.timeToX (t)) == Catch::Approx (t).margin (1.0e-6));

    // Zoom in 2x about the centre pixel keeps the time under it fixed.
    vm.zoomBy (2.0, 500.0);
    CHECK (vm.xToTime (500.0) == Catch::Approx (5.0).margin (1.0e-6));
    CHECK (vm.visibleSpanSecs() == Catch::Approx (5.0));

    // Scroll by +100 px shifts the window forward by 1 s (at the new 200 px/s).
    const double beforeStart = vm.viewStartSecs();
    vm.scrollByPixels (100.0);
    CHECK (vm.viewStartSecs() == Catch::Approx (beforeStart + 100.0 / vm.pixelsPerSecond()));

    // Auto-follow: a playhead past the right margin shifts the view; one inside does not.
    vm.setVisibleTimeRange (0.0, 10.0);
    vm.followPlayhead (5.0, 0.1);
    CHECK (vm.viewStartSecs() == Catch::Approx (0.0)); // 5 s is comfortably inside
    vm.followPlayhead (9.5, 0.1);
    CHECK (vm.viewStartSecs() == Catch::Approx (8.5)); // 9.5 past 9 s margin -> shift
    CHECK (vm.visibleSpanSecs() == Catch::Approx (10.0));
}

TEST_CASE ("Musical time: bars:beats, min:sec, and snapping at 120 bpm 4/4", "[render]")
{
    te::Engine engine { "EZStudio-timeline-test" };
    auto edit = daw::buildArrangementEdit (engine, 1, daw::EditPurpose::offlineRender);

    // Pin tempo/time-sig so the maths is exact: 120 bpm -> beat = 0.5 s, 4/4 bar = 2 s.
    edit->tempoSequence.getTempoAt (tracktion::TimePosition()).setBpm (120.0);
    auto& sig       = edit->tempoSequence.getTimeSigAt (tracktion::TimePosition());
    sig.numerator   = 4;
    sig.denominator = 4;

    SECTION ("readouts and snapping match the known grid")
    {
        CHECK (daw::barsBeatsText (*edit, 0.0).toStdString() == "1|1");
        CHECK (daw::barsBeatsText (*edit, 0.5).toStdString() == "1|2");
        CHECK (daw::barsBeatsText (*edit, 1.0).toStdString() == "1|3");
        CHECK (daw::barsBeatsText (*edit, 2.0).toStdString() == "2|1");

        CHECK (daw::minSecText (62.5).toStdString() == "1:02.500");
        CHECK (daw::minSecText (2.0).toStdString() == "0:02.000");

        // Bars at 0, 2, 4 s; beats every 0.5 s.
        CHECK (daw::snapSecondsToBar (*edit, 1.8) == Catch::Approx (2.0).margin (1.0e-6));
        CHECK (daw::snapSecondsToBar (*edit, 0.9) == Catch::Approx (0.0).margin (1.0e-6));
        CHECK (daw::snapSecondsToBeat (*edit, 0.4) == Catch::Approx (0.5).margin (1.0e-6));
        CHECK (daw::snapSecondsToBeat (*edit, 0.2) == Catch::Approx (0.0).margin (1.0e-6));
    }

    SECTION ("a clip snapped to a bar renders starting exactly at that bar")
    {
        // Source tone WAV.
        auto srcEdit = daw::buildSineToneEdit (engine, { 440.0, 0.5f, 0.5 }, daw::EditPurpose::offlineRender);
        auto srcWav  = daw::render_test::renderEditToWav (*srcEdit, 0.5, 48000.0);

        auto* track = te::getAudioTracks (*edit)[0];
        auto* clip  = daw::importAudioFileAsClip (*track, srcWav->getFile(), 1.8); // off the grid
        REQUIRE (clip != nullptr);

        const double snapped = daw::snapSecondsToBar (*edit, 1.8);
        CHECK (snapped == Catch::Approx (2.0).margin (1.0e-6)); // bar 2
        daw::moveClip (*clip, snapped);

        auto wav    = daw::render_test::renderEditToWav (*edit, 2.6, 48000.0);
        double sr   = 0.0;
        auto bufOpt = daw::render_test::readWavToBuffer (wav->getFile(), sr);
        REQUIRE (bufOpt.has_value());
        auto& buf = *bufOpt;

        const auto secs = [sr] (double s)
        { return (int) std::llround (s * sr); };
        const float before = daw::render_test::regionRmsDbfs (buf, 0, secs (0.1), secs (1.8));
        const float body   = daw::render_test::regionRmsDbfs (buf, 0, secs (2.05), secs (0.4));
        WARN ("[render] snap-to-bar: before=" << before << " body=" << body);
        CHECK (before < -90.0f);                     // silent up to the bar
        CHECK_THAT (body, WithinAbs (-9.03f, 0.6f)); // tone from the bar onward
    }
}
