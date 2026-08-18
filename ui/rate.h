// carts/jetchess/ui/rate.h
//
// The frame-rate readout: one number in the band above the panel, off until
// the player presses R, and the only thing on the screen that is not part of
// the game.
//
// IT BELONGS TO THE OVERLAY, not to the canvas. The number changes every
// frame, so folding it into the fingerprint would bake the whole screen a
// hundred times a second to redraw seven characters — which is the exact
// mistake jet_still.h's third law exists to prevent. It is therefore drawn
// after the restore and marks its own band, like the engine's readout and the
// piece under the pointer.
//
// THE NUMBER IS THE FIRMWARE'S. jet_draw_fps already knows what the machine's
// uncapped rate is and how to say it; this module decides only what it is set
// in and where it lands (config.h, "The frame-rate readout").
#ifndef JETCHESS_UI_RATE_H
#define JETCHESS_UI_RATE_H

#include "ui/view.h"

// Draw the readout if the player has asked for it, and mark what it painted.
// Called last of the three overlays, so the number sits over everything.
void jc_rate_paint_overlay(const JcView *view, JetStill *still);

#endif // JETCHESS_UI_RATE_H
