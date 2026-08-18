// carts/jetchess/art/pieces.h
//
// Which sprite cell holds which piece.
//
// The bake (scripts/jetchess_art.py, replayed by data/gfx/imports.ron) lays
// the set out as a set: one bank row per way of RIMMING it, each row in the
// engine's own order of kinds. So the cell for any piece is arithmetic rather
// than a table — and anybody who opens the Sprite Editor sees six tidy rows
// of chess pieces, which is the point.
//
// THERE ARE THREE LIVE ROWS AND NOT TWO. A white piece wears an ink rim, and
// ink reads against both squares. Bone does not: against the cream square it
// is very nearly the same colour, so a black piece rimmed in bone is a flat
// silhouette on one square and a glowing outlined object on the next, and the
// black army twinkles its way across the board. Black is therefore baked
// twice, once for each ground, and the board asks for the row that matches the
// square the piece is standing on.
//
// AND TWO HUSHED ONES. When a game ends or a pawn waits to be promoted, both
// square colours are carried half way to the room — and a piece left at full
// strength on that board does not merely stay put, it gets LOUDER: a white
// piece measures 3.6 to 1 on a live dark square and 5.9 on the dimmed one. A
// quiet board has to recede as ONE thing, so the last two rows are the same
// set with every colour through the identical multiply, rim and body alike.
// Black keeps its TWO grounds there as well. Hushing both of its rims to one
// put a 1.20-to-1 halo on the quiet light square, which is to say none — and
// the rule that stops the black army twinkling across the board has to survive
// the wash like everything else. White still takes one rim on any ground.
#ifndef JETCHESS_ART_PIECES_H
#define JETCHESS_ART_PIECES_H

#include "config.h"
#include "engine/chess.h"

// How many cells fit across the 1024-pixel sprite sheet at our cell size.
#define JC_CELLS_PER_ROW (1024 / JC_PIECE_PX) // 16

// The five bank rows, in the order scripts/jetchess_art.py writes them.
#define JC_ROW_WHITE 0
#define JC_ROW_BLACK_ON_DARK 1
#define JC_ROW_BLACK_ON_LIGHT 2
#define JC_ROW_WHITE_HUSHED 3
#define JC_ROW_BLACK_HUSHED_ON_DARK 4
#define JC_ROW_BLACK_HUSHED_ON_LIGHT 5

// The cell holding `piece` as it should look standing on a square of the given
// colour. `on_light` is only consulted for a BLACK piece; a white one has one
// rim and needs no choice on any ground. A piece in mid-air — carried under the pointer,
// or sliding home — asks for the dark-square row, whose bone rim reads against
// anything the piece may be passing over.
static inline unsigned jc_piece_cell(uint8_t piece, bool on_light, bool hushed) {
	const unsigned column = JC_KIND(piece) - JC_PAWN;
	const bool black = JC_COLOUR_OF(piece) == JC_BLACK;
	unsigned row;
	if (hushed) {
		row = !black                 ? JC_ROW_WHITE_HUSHED
		      : on_light             ? JC_ROW_BLACK_HUSHED_ON_LIGHT
		                             : JC_ROW_BLACK_HUSHED_ON_DARK;
	} else if (black) {
		row = on_light ? JC_ROW_BLACK_ON_LIGHT : JC_ROW_BLACK_ON_DARK;
	} else {
		row = JC_ROW_WHITE;
	}
	return row * JC_CELLS_PER_ROW + column;
}

#endif // JETCHESS_ART_PIECES_H
