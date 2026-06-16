// Phase 1, Chunk 8 — clip gesture regression in the GUI-capable daw_ui_tests target.
// Drives ArrangementView's gesture seams (the same methods the mouse handlers call) and
// checks the underlying model AND the painted result stay in lockstep:
//   import -> a clip exists at the drop time,
//   move   -> snaps to the beat grid, the model start moves, the snapshot shows the clip
//             at the new timeToX column, and undo reverts it,
//   trim   -> the model length changes,
//   fade   -> the model fade is set,
//   delete -> the clip is gone.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "engine/ArrangementEdit.h"
#include "engine/ArrangementRenderer.h"
#include "engine/EditUndo.h"
#include "engine/SineToneEdit.h"
#include "engine/TimelineViewModel.h"
#include "ui/ArrangementView.h"
#include "ui/ClipComponent.h"

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

TEST_CASE ("ArrangementView clip gestures drive the model and the painted view", "[snapshot]")
{
    te::Engine engine { "EZStudio-gesture-test" };

    // A 1.0 s source tone WAV via the daw_core exporter (no juce_dsp in this target).
    auto sineEdit = daw::buildSineToneEdit (engine, { 440.0, 0.5f, 1.0 }, daw::EditPurpose::offlineRender);
    juce::TemporaryFile srcWav (".wav");
    daw::ExportOptions srcOpts;
    srcOpts.endSecs = 1.0;
    REQUIRE (daw::exportEditToWav (*sineEdit, srcWav.getFile(), srcOpts).success);

    auto edit = daw::buildArrangementEdit (engine, 2, daw::EditPurpose::offlineRender);
    edit->tempoSequence.getTempoAt (tracktion::TimePosition()).setBpm (120.0); // beat = 0.5 s
    auto& sig       = edit->tempoSequence.getTimeSigAt (tracktion::TimePosition());
    sig.numerator   = 4;
    sig.denominator = 4;
    daw::ensureUndoManagerReady (*edit);

    daw::TimelineViewModel vm;
    vm.setWidthPixels (800);
    vm.setVisibleTimeRange (0.0, 8.0); // 100 px/s

    daw::ArrangementView view (*edit, vm);
    view.setSize (800, 200);

    auto* cc = view.importFileAt (srcWav.getFile(), 0, 2.0); // clip [2.0, 3.0] on track 0
    REQUIRE (cc != nullptr);
    CHECK (cc->clip().getPosition().getStart().inSeconds() == Catch::Approx (2.0).margin (1.0e-3));
    CHECK (cc->clip().getPosition().getLength().inSeconds() == Catch::Approx (1.0).margin (1.0e-3));

    SECTION ("move snaps to the beat grid and updates model + pixels, undo reverts")
    {
        view.selectClip (cc);
        view.moveClipTo (*cc, 3.1); // snaps to 3.0 s (beat grid) -> clip [3.0, 4.0]
        CHECK (cc->clip().getPosition().getStart().inSeconds() == Catch::Approx (3.0).margin (1.0e-6));

        const auto img = view.createComponentSnapshot (view.getLocalBounds());
        REQUIRE (img.isValid());

        const int y        = 24 + ((200 - 24) / 2) / 2; // ruler(24) + middle of track-0 lane
        const auto clipPix = img.getPixelAt (350, y);   // inside the moved clip [300,400]
        const auto gapPix  = img.getPixelAt (250, y);   // 2.5 s, now empty (clip moved away)
        CHECK ((int) clipPix.getBlue() > (int) clipPix.getRed() + 20);
        CHECK ((int) clipPix.getBlue() > (int) gapPix.getBlue() + 20);

        daw::undo (*edit);
        CHECK (cc->clip().getPosition().getStart().inSeconds() == Catch::Approx (2.0).margin (1.0e-6));
    }

    SECTION ("trim-right shortens the clip via the model")
    {
        view.selectClip (cc);
        view.trimClipRightTo (*cc, 2.5); // snaps to 2.5 s -> length 0.5 s
        CHECK (cc->clip().getPosition().getLength().inSeconds() == Catch::Approx (0.5).margin (0.02));
    }

    SECTION ("fade-in via the view sets the model fade")
    {
        view.selectClip (cc);
        view.setFadeInOnSelected (0.25);
        CHECK (cc->clip().getFadeIn().inSeconds() == Catch::Approx (0.25).margin (1.0e-3));
    }

    SECTION ("delete removes the clip from the track")
    {
        view.selectClip (cc);
        REQUIRE (te::getAudioTracks (*edit)[0]->getClips().size() == 1);
        view.deleteSelected();
        CHECK (te::getAudioTracks (*edit)[0]->getClips().isEmpty());
    }
}
