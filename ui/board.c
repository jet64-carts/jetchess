// carts/jetchess/ui/board.c
//
// The board, drawn (board.h). Everything here paints into whatever the current
// draw target is, which is the canvas during a bake and the screen during the
// overlay — the painters never need to know which.
#include "board.h"

#include "art/metrics.h"
#include "art/palette.h"
#include "art/pieces.h"
#include "config.h"
#include "jet64.h"
#include "jet_sprite.h"
#include "jet_text.h"
#include "ui/type.h"

// ---------------------------------------------------------------------------
// Where things are
// ---------------------------------------------------------------------------

void jc_board_square_origin(uint8_t square, bool flipped, int *x, int *y) {
	int file = JC_FILE_OF(square);
	int rank = JC_RANK_OF(square);
	if (flipped) {
		// Turning the board round is a half turn, not a mirror: both the files
		// and the ranks reverse, which is what keeps a1 a dark square.
		file = 7 - file;
		rank = 7 - rank;
	}
	*x = JC_BOARD_X + file * JC_SQUARE_PX;
	// Rank 1 is at the bottom of the screen, and y grows downward.
	*y = JC_BOARD_Y + (7 - rank) * JC_SQUARE_PX;
}

uint8_t jc_board_square_at(int x, int y, bool flipped) {
	const int column = (x - JC_BOARD_X) / JC_SQUARE_PX;
	const int row = (y - JC_BOARD_Y) / JC_SQUARE_PX;
	if (x < JC_BOARD_X || y < JC_BOARD_Y || column < 0 || column > 7 || row < 0 || row > 7) {
		return JC_NO_SQUARE;
	}
	int file = column;
	int rank = 7 - row;
	if (flipped) {
		file = 7 - file;
		rank = 7 - rank;
	}
	return JC_SQUARE(file, rank);
}

void jc_board_piece_origin(uint8_t square, bool flipped, int *x, int *y) {
	jc_board_square_origin(square, flipped, x, y);
	*x += JC_PIECE_INSET;
	*y += JC_PIECE_INSET;
}

// Is this a light square? A square is light when its file and rank have
// different parities — which is what makes h1 light and a1 dark.
static bool square_is_light(uint8_t square) {
	return ((JC_FILE_OF(square) + JC_RANK_OF(square)) & 1) != 0;
}

// A square's two colours, which every mark on the board is a version of. Light
// and dark are kept in a pair so no painter ever has to branch on it.
static uint8_t square_colour(uint8_t square, uint8_t light, uint8_t dark) {
	return square_is_light(square) ? light : dark;
}

// A RING ROUND A SQUARE: inset three, hairlined in ink on both edges, and as
// many pixels of colour between as the ring's importance earns. Every ring on
// the board is this shape, so a player learns one mark once.
//
// THEY DIFFER BY WEIGHT AND NOT BY HUE. Two ambers a stop apart cannot order
// themselves, because each one disappears on one of the two square colours:
// the accent at rest measures 1.06 to 1 against the dark square and the accent
// itself 1.24 against the light one, so on half the board the quieter mark was
// the louder. One pixel for the square you picked up FROM, three for the
// square you are pointing AT and for a king in check, and the order holds on
// any ground.
//
// The ink hairlines are what let one colour read on both squares at all: the
// shape's edge then has ten to one against its own fill instead of the one and
// a third it would have against the board.
static void paint_ring(int x, int y, uint8_t colour, bool hushed, int width) {
	const uint8_t hair = hushed ? JC_INK_HUSHED : JC_INK;
	jet_rect(x + JC_RING_INSET - 1, y + JC_RING_INSET - 1, JC_SQUARE_PX - 2 * JC_RING_INSET + 2,
	         JC_SQUARE_PX - 2 * JC_RING_INSET + 2, hair);
	for (int ring = 0; ring < width; ring++) {
		const int inset = JC_RING_INSET + ring;
		jet_rect(x + inset, y + inset, JC_SQUARE_PX - 2 * inset, JC_SQUARE_PX - 2 * inset, colour);
	}
	jet_rect(x + JC_RING_INSET + width, y + JC_RING_INSET + width,
	         JC_SQUARE_PX - 2 * (JC_RING_INSET + width),
	         JC_SQUARE_PX - 2 * (JC_RING_INSET + width), hair);
}

void jc_board_draw_piece(uint8_t piece, int x, int y, uint8_t square, bool hushed) {
	// A piece in mid-air asks for the bone rim, which reads against anything
	// it may be passing over (art/pieces.h).
	const bool on_light = square != JC_NO_SQUARE && square_is_light(square);
	jet_spr_mask_grid(jc_piece_cell(piece, on_light, hushed), x, y, false, false);
}

// ---------------------------------------------------------------------------
// The one shape the drawing API does not have
// ---------------------------------------------------------------------------

// A filled ellipse, which is what a shadow on the ground is. Solid, in a
// colour chosen for the square it lands on — NOT an ordered-dither veil. The
// machine has no alpha, so a half-strength anything is a pattern, and a
// pattern is the one rendering technique this cart does not otherwise use:
// every other edge on screen is anti-aliased through the palette's own ramps.
// One dithered object among them reads as a different program's work, and over
// a brass move-dot it reads as a speckled mess. Two named colours cost two
// palette entries and look like a shadow.
// The ellipse inscribed in a box. A BOX and not a centre-and-radius, because a
// filled circle drawn from an integer centre always covers an ODD number of
// pixels, and an eighty-pixel square's centre falls between two of them — so
// every disc on this board was half a pixel out and could not be anything
// else. Given the box, a 24-pixel dot lands exactly on 39.5 and a 51-pixel
// shadow exactly on the file.
//
// Solid, in a colour chosen for the ground it lands on — NOT an ordered-dither
// veil. The machine has no alpha, so a half-strength anything is a pattern,
// and a pattern is the one rendering technique this cart does not otherwise
// use: every other edge on screen is anti-aliased through the palette's own
// ramps. One dithered object among them reads as a different program's work.
// How wide this row of an ellipse is, in HALF-PIXELS, or nought where the
// shape does not reach. A row's pixel centres are odd half-pixels: when the
// box's width is even the centre is an even half-pixel, so every offset from
// it is odd and the row covers an even number of pixels; when the width is odd
// the offsets are even and the row is odd. Starting the walk on the right
// parity is the whole of it — starting it at nought always gave an odd row, so
// a 24-pixel dot once came out 24 tall and 23 wide.
static int ellipse_half(int w, int h, int row) {
	if (w <= 0 || h <= 0 || row < 0 || row >= h) {
		return 0;
	}
	const int rx2 = w, ry2 = h;
	// This row's distance from the centre, doubled, and the ellipse's own
	// equation in integers: no square roots, no floats, no surprises.
	const int dy2 = 2 * row + 1 - h;
	const int span = ry2 * ry2 - dy2 * dy2;
	if (span <= 0) {
		return 0; // the very top and bottom rows, which would be one speck
	}
	int half2 = (w & 1) ? 0 : 1;
	if (half2 * half2 * ry2 * ry2 > span * rx2 * rx2) {
		return 0; // not even half a pixel of ellipse on this row
	}
	while ((half2 + 2) * (half2 + 2) * ry2 * ry2 <= span * rx2 * rx2) {
		half2 += 2;
	}
	return half2;
}

static void fill_ellipse(int x, int y, int w, int h, uint8_t colour) {
	for (int row = 0; row < h; row++) {
		const int half2 = ellipse_half(w, h, row);
		if (half2 <= 0 && !(w & 1)) {
			continue;
		}
		jet_rectfill((2 * x + w - half2 - 1) / 2, y + row, half2 + 1, 1, colour);
	}
}

// ---------------------------------------------------------------------------
// The board itself
// ---------------------------------------------------------------------------

// THE COORDINATES, written in the band outside the board: the rank digits down
// its left-hand side, the file letters along its bottom, each one centred on
// the square it names (config.h, JC_COORD_BAND).
//
// ONE INK, ON ONE GROUND. Inside the board a mark landed on eight different
// grounds — two square colours, each plain or wearing the last move's tint,
// and both of those again on a quiet board — and no single ink cleared the
// contrast floor on all eight, so there were four of them, a function to
// choose between them, and three grounds on which the floor could not be
// reached by any ink the machine has. Out here the ground is the room, and the
// room never changes.
//
// So there is one ink, and it is not a new one. A file letter NAMES A COLUMN,
// which is the same job the scoresheet's own column heads do and the same job
// every section label in the panel does, and all three are therefore the same
// weight (ui/panel.c). The gate measures it on the ground it lands on, as it
// measures every other word in the cart.
//
// THE BOARD GOING QUIET DOES NOT REACH THEM, and that is the point rather than
// an oversight. The wash is what the BOARD does while a question is being
// asked on top of it; the panel does not answer it and neither does the room.
// A mark out here is furniture in the room, and furniture holds still.
static void paint_coordinates(const JcView *view) {
	const bool flipped = view->hand->flipped;
	// Every mark is placed from the square it names, through the one
	// conversion this file owns — which is what keeps it beside that row and
	// under that column however the board is turned round — and is then
	// centred on its own MEASURE, because the face is proportional and an `h`
	// and an `i` are different widths (ui/type.h).
	for (int index = 0; index < 8; index++) {
		char text[2] = {0, 0};
		int x, y;

		// The rank digit: centred across the band, and on its own row.
		jc_board_square_origin(JC_SQUARE(0, index), flipped, &x, &y);
		text[0] = (char)('1' + index);
		jc_print_ink_centre(text, JC_COORD_RANK_X, y + (JC_SQUARE_PX - JC_CAP_LABEL) / 2,
		                    JC_RUNG_COORD, JC_TEXT_GUTTER, 0);

		// ...and the file letter: centred on its own column, and down the band.
		jc_board_square_origin(JC_SQUARE(index, 0), flipped, &x, &y);
		text[0] = (char)('a' + index);
		jc_print_ink_centre(text, x + JC_SQUARE_PX / 2, JC_COORD_FILE_INK_Y, JC_RUNG_COORD,
		                    JC_TEXT_GUTTER, 0);
	}
}

// ---------------------------------------------------------------------------
// Promotion: the four pieces a pawn may become
// ---------------------------------------------------------------------------
//
// THE CARD IS FOUR BOARD SQUARES, and it obeys the board's rules exactly: the
// cells are squares of the board's own grid, on the pawn's own file, and each
// piece stands on the board's own baseline. It had a padded plate and a
// "PROMOTE TO" caption once; the plate put the card's top edge fourteen pixels
// from the screen's while every other margin is thirty-nine, and the caption
// told the player something four chess pieces appearing on the last rank had
// already said. Both are gone. What is left is a thing the eye has no rules to
// re-learn for.

// Queen first, because it is what nearly every promotion is.
static const uint8_t JC_PROMOTION_ORDER[JC_PROMOTE_CHOICES] = {JC_QUEEN, JC_ROOK, JC_BISHOP,
	JC_KNIGHT};

uint8_t jc_board_promotion_kind(int index) {
	return JC_PROMOTION_ORDER[index & (JC_PROMOTE_CHOICES - 1)];
}

bool jc_board_promotion_slot(const JcView *view, int index, int *x, int *y, int *size) {
	if (view->hand->state != JC_HAND_PROMOTING) {
		return false;
	}
	int square_x, square_y;
	jc_board_square_origin(view->hand->to, view->hand->flipped, &square_x, &square_y);
	// The four choices stack away from the edge the pawn arrived at, so the
	// column always fits on the board however the board is turned.
	const bool downward = square_y < JC_BOARD_Y + JC_BOARD_PX / 2;
	*size = JC_SQUARE_PX;
	*x = square_x;
	*y = square_y + (downward ? index : -index) * JC_SQUARE_PX;
	return true;
}

// The card's own corner: the first cell's, or the last one's when the column
// stacks upward.
bool jc_board_promotion_card(const JcView *view, int *x, int *y) {
	int first_x, first_y, last_x, last_y, size;
	if (!jc_board_promotion_slot(view, 0, &first_x, &first_y, &size) ||
	    !jc_board_promotion_slot(view, JC_PROMOTE_CHOICES - 1, &last_x, &last_y, &size)) {
		return false;
	}
	*x = first_x < last_x ? first_x : last_x;
	*y = first_y < last_y ? first_y : last_y;
	return true;
}

// One choice's piece, standing exactly where it would stand on a square: the
// same inset, the same baseline. Four pieces offered side by side is the one
// place their height ladder can be read directly, and a ladder only reads if
// they are all standing on the same floor.
static void paint_choice_piece(uint8_t piece, int x, int y, bool on_light) {
	jet_spr_mask_grid(jc_piece_cell(piece, on_light, false), x + JC_PIECE_INSET,
	                  y + JC_PIECE_INSET, false, false);
}

// The card's standing parts: the four cells and their pieces. Only the ring
// that follows the pointer is left to the overlay, so opening the card costs
// one bake and then almost nothing a frame.
static void paint_promotion_card(const JcView *view) {
	int card_x, card_y;
	if (!jc_board_promotion_card(view, &card_x, &card_y)) {
		return;
	}
	// ONE pixel of ink, INSIDE the column, on all four sides. Two pixels
	// outside put the card's top edge a pixel above the board's own frame and
	// two pixels over the files either side of it — a modal that leaves the
	// board it is modal to.
	// Filled with ink and then cut into four, so the seams between the cells
	// are the CARD's own colour. Drawing each cell inset by a pixel left the
	// two-pixel gaps showing the board through, which meant the seams changed
	// colour down the column and the card stopped being one object.
	jet_rectfill(card_x, card_y, JC_SQUARE_PX, JC_PROMOTE_CELLS_H, JC_INK);

	const uint8_t colour = view->match->position.side;
	const bool on_light = colour == JC_BLACK;
	const uint8_t ground = on_light ? JC_SQ_LIGHT : JC_SQ_DARK;
	for (int index = 0; index < JC_PROMOTE_CHOICES; index++) {
		int x, y, size;
		if (!jc_board_promotion_slot(view, index, &x, &y, &size)) {
			return;
		}
		// ONE GROUND for all four, divided by single pixels of the card's own
		// ink. Alternating them like board squares made the card read as four
		// squares that had failed to dim rather than as a menu: a chooser is a
		// control, and the first thing a control must do is group. The ground
		// is whichever square colour its four pieces read best on, so a white
		// promotion is offered on dark and a black one on light.
		const int top = (index == 0) ? 1 : 0;
		const int tall = size - top - (index == JC_PROMOTE_CHOICES - 1 ? 2 : 1);
		jet_rectfill(x + 1, y + top, size - 2, tall, ground);
		paint_choice_piece(JC_PIECE(colour, jc_board_promotion_kind(index)), x, y, on_light);
	}
}

// The BAND of an ellipse: the outer shape less the one `thickness` inside it,
// row by row. A ring and not a disc, so it can be laid around a piece without
// covering it — and from a box, like every other ellipse here, so it centres.
static void ring_ellipse(int x, int y, int w, int h, int thickness, uint8_t colour) {
	for (int row = 0; row < h; row++) {
		const int outer = ellipse_half(w, h, row);
		const int inner = ellipse_half(w - 2 * thickness, h - 2 * thickness, row - thickness);
		if (outer <= 0) {
			continue;
		}
		const int cx2 = 2 * x + w;
		if (inner <= 0) {
			jet_rectfill((cx2 - outer - 1) / 2, y + row, outer + 1, 1, colour);
			continue;
		}
		jet_rectfill((cx2 - outer - 1) / 2, y + row, (outer - inner) / 2, 1, colour);
		jet_rectfill((cx2 + inner + 1) / 2, y + row, (outer - inner) / 2, 1, colour);
	}
}

// Is the board stopped? It happens twice — when the game has ended, and while
// a pawn is waiting to be told what it becomes — and both look the same,
// because in both nothing on the board can be moved.
static bool board_is_quiet(const JcView *view) {
	return view->match->result != JC_RESULT_PLAYING ||
	       view->hand->state == JC_HAND_PROMOTING;
}

// What colour a square is painted, given everything it might be wearing. The
// order is the order of importance: a mated king outranks the last move, which
// outranks the plain square. Every one of them is a PAIR — a lit dark square
// stays darker than a lit light one — so lighting a square never dissolves the
// checkerboard into blocks.
//
// A FILL SAYS WHAT A SQUARE IS; A RING SAYS WHAT IS HAPPENING TO IT. The king
// in check, the square in the hand and the square under the pointer are all
// rings, drawn over whatever fill is underneath (jc_board_paint). That is the
// whole vocabulary, and it is why this function has three cases and not six.
static uint8_t square_paint(const JcView *view, uint8_t square) {
	const JcMatch *match = view->match;

	// A quiet board: both square colours carried half way to the room, so the
	// checkerboard is still a checkerboard and the whole thing has plainly
	// stopped. The one square that matters keeps its colour.
	if (board_is_quiet(view)) {
		if (match->result == JC_RESULT_CHECKMATE && square == jc_match_checked_king(match)) {
			return square_colour(square, JC_LIGHT_MATE, JC_DARK_MATE);
		}
		// The move that ended the game stays marked, dimmed with everything
		// else. A board that forgets the mating move while the scoresheet is
		// still naming it is a board that has stopped agreeing with the panel.
		if (match->ply_count > 0) {
			const JcMove last = match->played[match->ply_count - 1].move;
			if (square == last.from || square == last.to) {
				return square_colour(square, JC_LIGHT_LASTQUIET, JC_DARK_LASTQUIET);
			}
		}
		return square_colour(square, JC_LIGHT_QUIET, JC_DARK_QUIET);
	}


	if (match->ply_count > 0) {
		const JcMove last = match->played[match->ply_count - 1].move;
		if (square == last.from || square == last.to) {
			return square_colour(square, JC_LIGHT_LAST, JC_DARK_LAST);
		}
	}
	return square_colour(square, JC_SQ_LIGHT, JC_SQ_DARK);
}

// The mark that says a held piece may come here: a dot on an empty square, a
// ring around a piece that would be taken.
//
// Both are the accent, KEYLINED IN INK, and the keyline is the whole trick. No
// single colour has good contrast against both a cream square and a slate one;
// but a shape whose edge is ink has ten to one against its own fill wherever it
// lands, so it reads identically on both. This is the most important signal in
// the game and it is not allowed to be subtle.
static void paint_hint(const JcView *view, uint8_t square, bool is_capture) {
	int x, y;
	jc_board_square_origin(square, view->hand->flipped, &x, &y);

	if (!is_capture) {
		const int keyed = JC_HINT_DOT_PX + 2 * JC_HINT_KEYLINE_PX;
		fill_ellipse(x + (JC_SQUARE_PX - keyed) / 2, y + (JC_SQUARE_PX - keyed) / 2, keyed, keyed,
		             JC_INK);
		fill_ellipse(x + (JC_SQUARE_PX - JC_HINT_DOT_PX) / 2,
		             y + (JC_SQUARE_PX - JC_HINT_DOT_PX) / 2, JC_HINT_DOT_PX, JC_HINT_DOT_PX,
		             JC_BRASS);
		return;
	}
	// A ring round the piece it would take, and it obeys the board's own ring
	// law: inset three, hairlined in ink on both edges, and drawn from a BOX
	// so it centres on the square. Struck as circles from an integer centre it
	// came out an odd diameter — 65 pixels on 240 in a square whose centre is
	// 239.5 — and sat at inset eight while every other ring sat at three.
	const int outer = JC_SQUARE_PX - 2 * JC_RING_INSET;
	ring_ellipse(x + JC_RING_INSET, y + JC_RING_INSET, outer, outer, JC_HINT_KEYLINE_PX, JC_INK);
	const int mid = JC_RING_INSET + JC_HINT_KEYLINE_PX;
	ring_ellipse(x + mid, y + mid, JC_SQUARE_PX - 2 * mid, JC_SQUARE_PX - 2 * mid, JC_RING_PX,
	             JC_BRASS);
	const int inner = mid + JC_RING_PX;
	ring_ellipse(x + inner, y + inner, JC_SQUARE_PX - 2 * inner, JC_SQUARE_PX - 2 * inner,
	             JC_HINT_KEYLINE_PX, JC_INK);
}

void jc_board_paint(const JcView *view) {
	// One neutral hairline, a pixel outside the squares: the board's own edge,
	// and the only thing between it and the room. Not the accent — that has
	// exactly one job, and being furniture is not it.
	for (int ring = 1; ring <= JC_BOARD_FRAME_PX; ring++) {
		jet_rect(JC_BOARD_X - ring, JC_BOARD_Y - ring, JC_BOARD_PX + 2 * ring,
		         JC_BOARD_PX + 2 * ring, JC_CONTAINER_EDGE);
	}

	const uint8_t lifted = jc_view_lifted_square(view);
	const uint8_t king = jc_match_checked_king(view->match);
	const bool quiet = board_is_quiet(view);
	for (int rank = 0; rank < 8; rank++) {
		for (int file = 0; file < 8; file++) {
			const uint8_t square = JC_SQUARE(file, rank);
			int x, y;
			jc_board_square_origin(square, view->hand->flipped, &x, &y);
			jet_rectfill(x, y, JC_SQUARE_PX, JC_SQUARE_PX, square_paint(view, square));
			const uint8_t piece = view->match->position.board[square];
			if (piece != JC_EMPTY && square != lifted) {
				jc_board_draw_piece(piece, x + JC_PIECE_INSET, y + JC_PIECE_INSET, square,
				                    quiet);
			}
		}
	}

	// THE SQUARE THE PIECE CAME FROM wears a ring and not a flood. A flood is
	// six thousand four hundred pixels and a legal-move dot is four hundred:
	// painting the square you have already committed to as the largest mark on
	// the board, twenty times the area of the squares you may actually click,
	// is the loudness ladder upside down. It is also the one mark that could
	// not hold one strength on both parities — the accent at rest against a
	// dark square is a contrast of 1.06 to 1, which is a difference of hue and
	// nothing else, and vanishes for anybody who sees colour differently.
	if (view->hand->state != JC_HAND_EMPTY && view->hand->from != JC_NO_SQUARE && !quiet) {
		int x, y;
		jc_board_square_origin(view->hand->from, view->hand->flipped, &x, &y);
		paint_ring(x, y, JC_BRASS, false, JC_RING_HAND_PX);
	}

	// A king in check wears a ring as well as a tint, and a MATED king wears
	// the same two through the same wash as the rest of the board. One object
	// at two strengths, not two objects: "answer this" and "this is over" are
	// the same fact at different volumes, and the panel says which. The ring is
	// the DEEP alert and not the alert itself, because against the tint it sits
	// on the plain one is a contrast of 1.35 to 1 and reads as a soft edge.
	// A MATED KING'S SQUARE IS THE FILL AND NOTHING ELSE. It wore the check
	// ring as well, hushed with everything around it — and a ring at 1.56 to
	// one against the fill it sits on is not a ring, it is a slightly
	// different red. Check is a ring on a plain square, mate is a filled
	// square: two marks, not one mark at two volumes.
	if (king != JC_NO_SQUARE && !quiet) {
		int x, y;
		jc_board_square_origin(king, view->hand->flipped, &x, &y);
		paint_ring(x, y, JC_ALERT_DEEP, false, JC_RING_PX);
	}

	// The held piece's moves, drawn over the pieces so a ring sits around the
	// piece it would take rather than under it.
	if (view->hand->state == JC_HAND_HOLDING) {
		for (int i = 0; i < view->hand->options.count; i++) {
			const JcMove move = view->hand->options.moves[i];
			// A promotion offers four moves to one square; one mark will do.
			if (move.flags & JC_MOVE_PROMOTION && move.promotion != JC_QUEEN) {
				continue;
			}
			const bool capture = (move.flags & JC_MOVE_CAPTURE) != 0;
			// THE DOT UNDER THE PIECE IN THE HAND IS NOT DRAWN. The held piece
			// is carried at the pointer, so on the square the pointer is over
			// it sits directly on top of that square's dot — and the arc of
			// brass that shows past the piece's base reads as part of the
			// piece rather than as a mark on the board. The hover ring is
			// already round that square saying the same thing, so the dot is
			// redundant there as well as ugly. A capture's RING is kept: it
			// lies at the square's edge, well clear of any piece's base, and
			// "this one you take" is not a thing to hide at the moment of
			// taking it.
			if (!capture && move.to == view->hand->hover) {
				continue;
			}
			paint_hint(view, move.to, capture);
		}
	}
	paint_coordinates(view);

	// A pawn on the far rank, on the quiet board painted above: the card that
	// asks the question, standing on top of it.
	if (view->hand->state == JC_HAND_PROMOTING) {
		paint_promotion_card(view);
	}
}

// ---------------------------------------------------------------------------
// The overlay: everything that moves
// ---------------------------------------------------------------------------

// Where a sliding piece is, this frame. The slide is only the picture catching
// up: the move has already been played, and this is the eye being shown how.
static void slide_position(const JcView *view, int *x, int *y) {
	const JcSlide *slide = view->slide;
	int from_x, from_y, to_x, to_y;
	jc_board_piece_origin(slide->move.from, view->hand->flipped, &from_x, &from_y);
	jc_board_piece_origin(slide->move.to, view->hand->flipped, &to_x, &to_y);
	float t = slide->duration > 0.0f ? slide->elapsed / slide->duration : 1.0f;
	t = jet_clampf(t, 0.0f, 1.0f);
	// Smoothstep: no acceleration at either end, which is what makes a moved
	// piece look placed rather than fired.
	const float eased = t * t * (3.0f - 2.0f * t);
	*x = from_x + (int)((float)(to_x - from_x) * eased);
	*y = from_y + (int)((float)(to_y - from_y) * eased);
}

// A piece drawn as though held above the board: a shadow on the GROUND under
// it, then the piece itself, higher.
//
// THE SHADOW BELONGS TO THE SQUARE, NOT TO THE PIECE. That is the whole of it.
// A shadow computed from where the piece is drawn travels up with the piece,
// arrives a pixel under its foot, and reads as a smudge glued to the base —
// there is no parallax, so there is no height. Taken from the square the
// pointer is over, it stays on the ground while the piece floats above it, and
// the gap between them IS the lift.
//
// It is an ellipse and not an offset copy of the sprite, because a flat
// silhouette displaced down and across is exactly what a second piece standing
// behind the first looks like. It is allowed to fall outside the square, since
// a shadow that stops at an edge is a sticker.
static void paint_lifted(JetStill *still, uint8_t piece, int x, int y, uint8_t ground,
                         bool flipped) {
	const int width = JC_PIECE_INK_WIDTH[JC_KIND(piece)] + JC_LIFT_SHADOW_GROW;
	if (ground != JC_NO_SQUARE) {
		int square_x, square_y;
		jc_board_square_origin(ground, flipped, &square_x, &square_y);
		fill_ellipse(square_x + (JC_SQUARE_PX - width) / 2,
		             square_y + JC_PIECE_INSET + JC_PIECE_FOOT_ROW - JC_LIFT_SHADOW_H + 1, width,
		             JC_LIFT_SHADOW_H, square_colour(ground, JC_LIGHT_LIFT, JC_DARK_LIFT));
		jet_still_mark(still, square_x, square_y, JC_SQUARE_PX, JC_SQUARE_PX);
	}
	// In the air, so the rim that reads against anything.
	jc_board_draw_piece(piece, x, y - JC_LIFT_PX, JC_NO_SQUARE, false);
	// One rectangle over the piece at its lifted height, with slack all round
	// (jet_still.h, law four).
	jet_still_mark(still, x - 2, y - JC_LIFT_PX - 2, JC_PIECE_PX + 4, JC_PIECE_PX + 4);
}

void jc_board_paint_overlay(const JcView *view, JetStill *still) {
	const JcHand *hand = view->hand;

	// The ring that says which square the pointer is over. Drawn here and not
	// baked, because it changes whenever the pointer crosses a square and
	// re-baking the screen for a rectangle is a great deal of work. (The
	// hover square IS in the fingerprint while a piece is held, for the one
	// thing the canvas draws differently for it — the dot underneath the piece
	// — and the ring is still the overlay's either way.)
	if (hand->hover != JC_NO_SQUARE && hand->state != JC_HAND_PROMOTING) {
		int x, y;
		jc_board_square_origin(hand->hover, hand->flipped, &x, &y);
		paint_ring(x, y, JC_BRASS, false, JC_RING_PX);
		jet_still_mark(still, x, y, JC_SQUARE_PX, JC_SQUARE_PX);
	}

	if (view->slide->running) {
		int x, y;
		slide_position(view, &x, &y);
		paint_lifted(still, view->slide->piece, x, y, view->slide->move.to, hand->flipped);
	}

	if (hand->state == JC_HAND_HOLDING) {
		const uint8_t piece = view->match->position.board[hand->from];
		int x, y;
		if (hand->dragging) {
			// The piece travels with the pointer, held where it was picked up
			// rather than snapped to its centre — which is what makes it feel
			// like a piece and not a cursor.
			x = hand->pointer_x - hand->grab_x;
			y = hand->pointer_y - hand->grab_y;
		} else {
			jc_board_piece_origin(hand->from, hand->flipped, &x, &y);
		}
		if (piece != JC_EMPTY) {
			paint_lifted(still, piece, x, y, hand->hover, hand->flipped);
		}
	}

	// The card itself was baked into the canvas when the hand entered this
	// state (jc_view_fingerprint carries hand->state), so all that is left to
	// do every frame is move one ring. The cell under it is restored from the
	// canvas first, which is what makes the ring cost a hundred-and-sixty
	// pixels instead of a plate.
	if (hand->state == JC_HAND_PROMOTING) {
		int x, y, size;
		if (hand->hover_promotion >= 0 &&
		    jc_board_promotion_slot(view, hand->hover_promotion, &x, &y, &size)) {
			// The board's ring, not a second one. A card built out of board
			// squares marks a choice the way the board marks a square.
			paint_ring(x, y, JC_BRASS, false, JC_RING_PX);
			jet_still_mark(still, x, y, size, size);
		}
	}
}
