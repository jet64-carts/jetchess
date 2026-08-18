// carts/jetchess/ui/ink.c
//
// The voices, built (ink.h).
#include "ink.h"

#include "art/palette.h"
#include "config.h"
#include "jet_palette.h"

// The five colours JetChess sets a word in: the panel's four reading weights,
// and the accent.
//
// There were NINE. The four extra were the board's own corner marks, two per
// square colour, because a mark that had to read on a plain square and on a
// marked one could not do both in one ink. The marks now stand in the band
// outside the board, where the ground is the room and never anything else, and
// a rank digit is simply a label — so four voices, four tables and the
// function that chose between them all went with them (ui/board.c).
const uint8_t JC_INK_VOICE[] = {
	JC_TEXT,
	JC_TEXT_DIM,
	JC_TEXT_FAINT,
	JC_TEXT_GUTTER,
	JC_BRASS,
};

// Counted from the list itself, so the two can never disagree — and a constant
// expression, so it is also the height of the table below.
#define JC_INK_VOICE_COUNT ((int)(sizeof JC_INK_VOICE / sizeof JC_INK_VOICE[0]))

const int JC_INK_VOICES = JC_INK_VOICE_COUNT;

// One table a voice, and one byte a palette entry saying which voice it is —
// so asking a colour for its table is an index and never a search. The
// sentinel is what makes an undeclared colour answer NULL rather than quietly
// borrowing the first voice in the list.
#define JC_INK_NONE 0xFFu

static uint8_t s_map[JC_INK_VOICE_COUNT][JET_PALETTE_SIZE];
static uint8_t s_voice_of[JET_PALETTE_SIZE];

void jc_ink_init(void) {
	for (int entry = 0; entry < JET_PALETTE_SIZE; entry++) {
		s_voice_of[entry] = JC_INK_NONE;
	}
	for (int voice = 0; voice < JC_INK_VOICE_COUNT; voice++) {
		const uint8_t colour = JC_INK_VOICE[voice];
		JetRgb ink;
		jet_palette_get(colour, &ink.r, &ink.g, &ink.b);
		jet_palette_ink_map(s_map[voice], JC_INK_RAMP_FOOT, JC_INK_RAMP_HEAD, ink);
		s_voice_of[colour] = (uint8_t)voice;
	}
}

const uint8_t *jc_ink_map(uint8_t colour) {
	const uint8_t voice = s_voice_of[colour];
	return voice == JC_INK_NONE ? NULL : s_map[voice];
}
