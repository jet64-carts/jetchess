// carts/jetchess/ui/rate.c
//
// The frame-rate readout, drawn (rate.h).
#include "rate.h"

#include "art/palette.h"
#include "config.h"
#include "jet64.h"
#include "jet_camera.h"
#include "jet_still.h"
#include "ui/type.h"

void jc_rate_paint_overlay(const JcView *view, JetStill *still) {
	if (!view->hand->showing_rate) {
		// Nothing is drawn and nothing is marked — and nothing needs to be.
		// The pixels the readout last left in each buffer are put back on that
		// buffer's own next visit, from the mark made when they were painted,
		// so switching it off cleans up after itself in three frames without a
		// line of code here (jet_still.h, law two).
		return;
	}

	// jet_draw_fps takes no position: it sets the reading hard into the
	// screen's own top-right corner, JC_RATE_FIRMWARE_PAD pixels in. The
	// camera is what carries it from there onto this cart's grid — every
	// drawing call takes the camera's offset off the position it was given, so
	// an offset of "where the firmware puts it, less where the layout wants
	// it" moves the reading without re-implementing a character of it.
	//
	// Both offsets are constant whatever the number reads, because the reading
	// is right-aligned: only its LEFT edge moves as a digit is gained or lost.
	jet_cam_set(JC_SCREEN_W - JC_RATE_FIRMWARE_PAD - JC_RATE_RIGHT,
	            JC_RATE_FIRMWARE_PAD + JC_INK_INSTRUMENT - JC_RATE_INK_Y);
	const TextFormat format = jc_rung_format(JC_RUNG_READOUT, JC_TEXT_DIM, 0);
	jet_draw_fps(&format);
	jet_cam_reset();

	// What the next visit to this buffer must put back: the band the reading
	// is set in, anchored at the right edge it is aligned to and made as wide
	// as the widest reading the machine could print. A band that cannot be
	// outgrown costs what a tight one would, and a band a pixel too small
	// leaves a smear that outlives the frame that made it (jet_still.h, law
	// four).
	const int width = JC_RATE_MAX_CHARS * JC_SFONT_INSTRUMENT_CELL;
	jet_still_mark(still, JC_RATE_RIGHT - width, JC_RATE_INK_Y - JC_INK_INSTRUMENT, width,
	               JC_SFONT_INSTRUMENT_CELL);
}
