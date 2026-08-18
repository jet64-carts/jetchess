// carts/jetchess/ui/type.c
//
// The type ladder, set (type.h).
//
// Five short functions, and every word JetChess draws goes through one of
// them. That is the point: the moment a caller reaches past these for
// `jet_print` it has stopped positioning by ink and stopped asking the palette
// for its voice, and nothing on screen will say so until somebody measures it.
#include "type.h"

#include "ui/ink.h"

// One face, from the four numbers config.h holds it as, plus the two things a
// bake cannot carry: the word space this interface wants, and the ring the
// runtime must not advance.
//
// `JET_SFONT_DEFAULTS` asks the runtime to measure each glyph's advance from
// its baked pixels, which is the whole of "proportional". Declaring the ring
// keeps that measure the letterform's own — advances come from inside the
// ring, and the ring overhangs the pen the way a drop shadow does. A space has
// no pixels to measure, so the runtime would default it to half a cell, which
// on advances this tight reads as a gap rather than as a word break: a quarter
// of the em is the classical word space, and the em of a bitmap face is the
// height its letters were drawn at, not the cell they were dropped into.
static void register_face(uint8_t slot, uint32_t index, int cell, int em) {
	const JetSpriteFontDef face = {
		.grid_index = index,
		.cell_w = (uint16_t)cell,
		.cell_h = (uint16_t)cell,
		.flags = JET_SFONT_DEFAULTS,
		.space_width = (uint8_t)(em / 4),
		.outline_w = JC_TYPE_OUTLINE_PX,
	};
	(void)jet_sprite_font_set(slot, &face);
}

void jc_type_init(void) {
	register_face(JC_SFONT_LABEL, JC_SFONT_LABEL_INDEX, JC_SFONT_LABEL_CELL, JC_SFONT_LABEL_EM);
	register_face(JC_SFONT_INSTRUMENT, JC_SFONT_INSTRUMENT_INDEX, JC_SFONT_INSTRUMENT_CELL,
	              JC_SFONT_INSTRUMENT_EM);
	register_face(JC_SFONT_TEXT, JC_SFONT_TEXT_INDEX, JC_SFONT_TEXT_CELL, JC_SFONT_TEXT_EM);
	register_face(JC_SFONT_STATEMENT, JC_SFONT_STATEMENT_INDEX, JC_SFONT_STATEMENT_CELL,
	              JC_SFONT_STATEMENT_EM);
	// The voices the four of them speak in, built from the palette that is
	// already live by the time a guest runs.
	jc_ink_init();
}

TextFormat jc_rung_format(JcRung rung, uint8_t colour, int letterspacing) {
	// A SPRITE FACE ADVANCES BY THE LETTERFORM ALONE: the side bearings the
	// typographer drew are not in the bake, and JC_TRACK is what puts them
	// back. A Font ROM glyph sits in a cell that already holds its own, so the
	// logotype takes none — which is also why its kerning constants
	// (JC_WORDMARK_KERN) still mean what they measured.
	const bool baked = rung.font >= JET_FONT_BANK_COUNT;
	const int track = letterspacing + (baked ? JC_TRACK : 0);
	TextFormat format = jet_make_text_format(rung.font, colour, rung.size, track, 0, 1.0f, 0, 0);
	// The voice. A sprite face reads this and ignores `color`; the ROM face
	// reads `color` and ignores this. Setting both here is what lets every
	// call site name one colour and mean it whichever face answers.
	format.palette_map = jc_ink_map(colour);
	return format;
}

void jc_print_ink(const char *text, int x, int ink_y, JcRung rung, uint8_t colour) {
	TextFormat format = jc_rung_format(rung, colour, 0);
	jet_print_ext(text, x, ink_y - rung.ink, &format);
}

// Ending at `right`, which is how every value is set: the figures then line up
// down the column however many digits they have.
void jc_print_ink_right(const char *text, int right, int ink_y, JcRung rung, uint8_t colour) {
	TextFormat format = jc_rung_format(rung, colour, 0);
	jet_print_ext(text, right - jet_text_width(text, &format), ink_y - rung.ink, &format);
}

// Centred on its own MEASURE, never on a character count times a cell width:
// four of the five faces here are proportional.
void jc_print_ink_centre(const char *text, int centre_x, int ink_y, JcRung rung, uint8_t colour,
                         int letterspacing) {
	TextFormat format = jc_rung_format(rung, colour, letterspacing);
	jet_print_ext(text, centre_x - jet_text_width(text, &format) / 2, ink_y - rung.ink, &format);
}
