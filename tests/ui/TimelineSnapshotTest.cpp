// Phase 1, Chunk 7 — timeline component snapshot/geometry regression. Runs in the
// GUI-capable daw_ui_tests target (links daw_ui). It builds a known one-clip arrangement,
// lays out a TimelineComponent via the TimelineViewModel, and checks BOTH:
//   * component geometry: the clip's pixel bounds equal timeToX(clip start/end), and
//   * a real createComponentSnapshot: the clip's pixels are clip-coloured where the model
//     says the clip is, and the lane is background-coloured before it.
// Geometry/pixel invariants are used (not a golden PNG) so font/AA differences across
// machines can't make it flaky. createComponentSnapshot works headless because TestMain
// holds a process-wide ScopedJuceInitialiser_GUI.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "engine/ArrangementEdit.h"
#include "engine/ArrangementRenderer.h"
#include "engine/ClipImporter.h"
#include "engine/SineToneEdit.h"
#include "engine/TimelineViewModel.h"
#include "ui/TimelineComponent.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

TEST_CASE ("TimelineComponent lays out and paints a clip where the model says", "[snapshot]")
{
    te::Engine engine { "EZStudio-snapshot-test" };

    // A real source WAV (so the clip + its thumbnail are valid). Rendered via the daw_core
    // exporter to avoid pulling juce_dsp into this GUI test target.
    auto sineEdit = daw::buildSineToneEdit (engine, { 440.0, 0.5f, 0.5 }, daw::EditPurpose::offlineRender);
    juce::TemporaryFile srcWav (".wav");
    daw::ExportOptions srcOpts;
    srcOpts.endSecs = 0.5; // the tone synth has no clips, so edit.getLength() is 0 -- bound it explicitly
    REQUIRE (daw::exportEditToWav (*sineEdit, srcWav.getFile(), srcOpts).success);

    auto edit                  = daw::buildArrangementEdit (engine, 2, daw::EditPurpose::offlineRender);
    auto* track                = te::getAudioTracks (*edit)[0];
    constexpr double clipStart = 1.0; // clip spans [1.0, 1.5]
    auto* clip                 = daw::importAudioFileAsClip (*track, srcWav.getFile(), clipStart);
    REQUIRE (clip != nullptr);

    // 800 px over [0, 4) s -> 200 px/s; clip at 1.0 s -> x = 200, end 1.5 s -> x = 300.
    daw::TimelineViewModel vm;
    vm.setWidthPixels (800);
    vm.setVisibleTimeRange (0.0, 4.0);

    daw::TimelineComponent timeline (*edit, vm);
    timeline.setSize (800, 200); // 2 tracks -> 100 px lanes

    // --- geometry: clip bounds == timeToX(start/end) ---
    REQUIRE (timeline.clipComponents().size() == 1);
    auto* cc = timeline.clipComponents()[0];
    CHECK (cc->getX() == Catch::Approx (vm.timeToX (clipStart)).margin (1.0));
    CHECK (cc->getRight() == Catch::Approx (vm.timeToX (1.5)).margin (1.0));
    CHECK (cc->getY() == 0); // track 0 lane

    // --- snapshot: pixels match the geometry ---
    const auto img = timeline.createComponentSnapshot (timeline.getLocalBounds());
    REQUIRE (img.isValid());
    CHECK (img.getWidth() == 800);

    const int laneMidY  = timeline.trackHeight() / 2;     // inside track 0's lane
    const auto clipPix  = img.getPixelAt (250, laneMidY); // centre of the clip (x in [200,300])
    const auto emptyPix = img.getPixelAt (60, laneMidY);  // before the clip -> lane background

    // The clip body is blue-ish (more blue than red); the empty lane is neutral grey.
    CHECK ((int) clipPix.getBlue() > (int) clipPix.getRed() + 20);
    CHECK (std::abs ((int) emptyPix.getBlue() - (int) emptyPix.getRed()) < 16);
    // And the clip pixel is clearly different from the empty-lane pixel.
    CHECK ((int) clipPix.getBlue() > (int) emptyPix.getBlue() + 20);
}
