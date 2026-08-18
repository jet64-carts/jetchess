// carts/jetchess/ui/panel.c
//
// The panel, drawn (panel.h).
//
// Everything a player needs to know and nothing else. Four rules hold it
// together, and between them they are the whole design:
//
//   THE TYPE IS A LADDER, and its steps were measured rather than chosen —
//   the logotype at 28, what the panel says out loud at 18, its values at 14,
//   the scoresheet and the footnote at 11, its labels at 9, in cap heights
//   (config.h). No two things that mean different things are the same size,
//   and a value is never set at its label's height.
//
//   THE SPACING IS BETWEEN INK, not between glyph cells. A cell is as tall as
//   its face's line height and its letter is not, so five faces spaced by
//   cells read as five separate decisions. Every rung therefore carries where
//   its ink begins, and `jc_print_ink` (ui/type.h) prints so the INK lands —
//   which is why nothing in this file calls `jet_print` itself, and why the
//   whole column down this panel is arithmetic rather than nine measured
//   offsets (config.h, "The blocks down the panel").
//
//   THE ACCENT MARKS WHAT IS LIVE, and nothing else: what the player may do
//   this instant, and what has just happened. The move at the foot of the
//   scoresheet, the opening the position has this moment become, the score
//   line of a game that has just ended. A setting is not live; a label is not
//   live; furniture is never the accent. The one exception is the first word
//   of the logotype, which is a name.
//
//   EVERY BLOCK IS A LABEL AND A THING, one gap between the two and one
//   between blocks. Nothing here is positioned by eye.
#include "panel.h"

#include "art/metrics.h"
#include "art/palette.h"
#include "art/pieces.h"
#include "config.h"
#include "engine/search.h"
#include "jet64.h"
#include "jet_sprite.h"
#include "jet_text.h"
#include "ui/line.h"
#include "ui/type.h"

// The small capital that heads a block, with a tick before it. The tick is
// what turns a stack of paragraphs into a stack of SECTIONS, and it is the
// same three pixels every time — which is what makes the column read as one
// design rather than as five decisions. It is furniture, so it is not the
// accent's colour; but furniture at 1.7 to one is decoration that failed
// rather than furniture, so it carries the same weight a control's edge does.
// There are exactly two furniture weights in the panel and one dead one: a
// BOX's edge, a CONTROL's edge, and a control that cannot be used, which takes
// the box weight. Four greys within ten per cent of each other read as noise
// rather than as a system.
static void paint_label(const char *text, int ink_y) {
	jet_rectfill(JC_BLOCK_X, ink_y, JC_LABEL_TICK_W, JC_CAP_LABEL, JC_TEXT_GUTTER);
	// The same weight as the tick beside it and the column heads in the move
	// list, because they are the same job. A section label set in the value
	// colour made the label and the value it heads the same ink, which is the
	// panel's own rule saying nothing — but demoting it to a control's edge
	// weight put it under four and a half to one, which is a floor no text in
	// this cart goes below. This value is a clear step down from a value's and
	// still comfortably above it.
	TextFormat label = jc_rung_format(JC_RUNG_LABEL, JC_TEXT_GUTTER, JC_LABEL_TRACK);
	jet_print_ext(text, JC_BLOCK_X + JC_LABEL_TICK_W + JC_LABEL_TICK_GAP, ink_y - JC_INK_LABEL,
	              &label);
}

// ---------------------------------------------------------------------------
// The blocks, top to bottom
// ---------------------------------------------------------------------------

// The wordmark, in two tones — the machine's name in the accent, the game's in
// the reading colour — and, at the far end of the same line, one mark that
// says what state the game is in: a disc of the moving side's own piece colour
// while it runs, and the score line once it has ended.
static void paint_wordmark(const JcView *view) {
	const int ink_y = JC_PANEL_Y + JC_PANEL_WORDMARK_Y;
	// THREE PRINTS, because the T has a bearing on BOTH sides. The face is
	// monospaced and its T is eight pixels narrower than its cell, so the
	// letter carries four pixels of air to its left and four to its right.
	// Closing only the right-hand one left an eight-pixel gap at E|T while
	// every other gap was four — the widest space in the logotype falling
	// INSIDE the first word, so it read "JE TCHESS". The T is pulled back by
	// its left bearing and the second word by both.
	TextFormat display = jc_rung_format(JC_RUNG_WORDMARK, JC_BRASS, 0);
	const int print_y = ink_y - JC_INK_MARK;
	jet_print_ext("JE", JC_BLOCK_X, print_y, &display);
	const int tee_x = JC_BLOCK_X + jet_text_width("JE", &display) - JC_WORDMARK_KERN_T;
	jet_print_ext("T", tee_x, print_y, &display);
	jet_text_set_color(&display, JC_TEXT);
	jet_print_ext("CHESS", tee_x + jet_text_width("T", &display) - JC_WORDMARK_KERN, print_y,
	              &display);

	const char *score = jc_match_score_line(view->match);
	if (score) {
		// Right-aligned flush on the column's own spine. That is only true
		// because the baked face advances by its letterform: a monospaced one
		// stops short of its edge by whatever bearing its last cell holds, and
		// this call used to add three pixels back by hand. It matters here
		// more than anywhere else in the panel, because the thing that
		// occupies this identical slot in the other state — the turn disc —
		// lands on the spine by its ink, so a shortfall would move the slot's
		// right edge as the game ended.
		jc_print_ink_right(score, JC_BLOCK_RIGHT,
		                   ink_y + (JC_CAP_MARK - JC_CAP_STATEMENT) / 2, JC_RUNG_RESULT,
		                   JC_BRASS);
		return;
	}
	const bool white_to_move = view->match->position.side == JC_WHITE;
	// A circle of radius r covers 2r+1 pixels, and JC_BLOCK_RIGHT is the first
	// pixel PAST the column — so the centre is a radius in from the last one,
	// not from the edge, or the disc hangs a pixel over it. Its y is the
	// middle of the wordmark's ink, which is not the middle of its box.
	const int disc_x = JC_BLOCK_RIGHT - 1 - JC_TURN_DISC_R;
	const int disc_y = ink_y + (JC_CAP_MARK - 1) / 2;
	jet_circfill(disc_x, disc_y, JC_TURN_DISC_R, white_to_move ? JC_INK : JC_BONE);
	jet_circfill(disc_x, disc_y, JC_TURN_DISC_R - 1,
	             white_to_move ? JC_WHITE_PIECE : JC_BLACK_PIECE);
}

static void paint_rule(int y) {
	jet_rectfill(JC_BLOCK_X, y, JC_BLOCK_W, 1, JC_CONTAINER_EDGE);
}

// The one sentence that changes and matters, set in the display face: the
// headline of the panel's body. Kept short enough that the longest of them
// fits the column, which is what buys it the size.
static void paint_status(const JcView *view) {
	// A BLOCKING STATE OWNS THIS LINE. While a pawn waits to be told what it
	// becomes, the game is not waiting for White to move — it is waiting for
	// one click, and three buttons have just gone grey with no reason given. A
	// panel that goes on saying "White to move" through that is lying at the
	// one moment the player needs it.
	const char *line = view->hand->state == JC_HAND_PROMOTING ? "Choose a piece"
	                                                          : jc_match_status(view->match);
	jc_print_ink(line, JC_BLOCK_X, JC_PANEL_Y + JC_PANEL_STATUS_Y, JC_RUNG_STATUS, JC_TEXT);
}

static void paint_opening(const JcView *view) {
	const char *opening = view->match->opening;
	paint_label("OPENING", JC_PANEL_Y + JC_PANEL_OPENING_Y);
	// Three states and not two. Before the first move nothing has been played,
	// so saying the game has left the books is simply untrue — the starting
	// position is the first line of every one of them. After it, either the
	// position has a name or it has not.
	//
	// Named or not, the line is set at ONE size and one weight: a slot that
	// swings from invisible to loud between move two and move three reads as
	// a fault rather than as news. Only the colour moves, dim to bright —
	// which is the SAME escalation the material line makes two blocks down,
	// and two sibling read-outs must not resolve the same transition in two
	// different ways.
	const char *line = opening                      ? opening
	                   : view->match->ply_count == 0 ? "Any game you like"
	                                                 : "Not in the books";
	jc_print_ink(line, JC_BLOCK_X, JC_PANEL_Y + JC_PANEL_OPENING_VALUE_Y, JC_RUNG_VALUE,
	             opening ? JC_TEXT : JC_TEXT_DIM);
}

// The pieces the leader is up, largest first, drawn at twenty-four pixels from
// the sixty-four the sprite was baked at. They line up with the END OF THE BAR
// that names their side — White's at the right, Black's at the left — which is
// the same left-and-right the bar itself uses, so the strip needs no label of
// its own. They share a baseline without being asked to: every piece's ink
// ends on the same row of its cell, so scaling the cell scales the baseline
// with it.
//
// It is drawn on the panel's dark ground and never on a quiet board, so every
// piece takes the rim a live DARK square would give it: white pieces ringed in
// ink, black in bone. The king is
// in the loop because the loop is "largest first" and not because a king can
// ever be spare.
static void paint_captures(const JcView *view, int bar_x) {
	uint8_t spare[JC_KINDS];
	const uint8_t leader = jc_match_advantage(view->match, spare);
	if (leader >= JC_COLOURS) {
		return;
	}
	const uint8_t taken = leader == JC_WHITE ? JC_BLACK : JC_WHITE;
	const float scale = (float)JC_CAPTURE_PX / (float)JC_PIECE_PX;
	SpriteFormat format = jet_make_sprite_format(scale, scale, 0.0f, 1.0f, false, false, true);
	const int y = JC_PANEL_Y + JC_PANEL_CAPTURES_Y;
	int reach = 0;
	for (int kind = JC_KING; kind >= JC_PAWN; kind--) {
		for (int n = 0; n < spare[kind]; n++) {
			// A SCALED SPRITE IS PLACED BY ITS CENTRE. jet_spr_ext scales
			// and turns about the sprite's own middle, and (x, y) is where
			// that middle goes — pass a corner and the whole strip lands half
			// a cell up and to the left of the slot it was meant for, which is
			// exactly what it did: the first pawn hung off the end of the bar.
			// SPACED BETWEEN INK, like every other row in this panel. Stepped
			// by the CELL the gaps closed up before a wide piece and opened
			// before a narrow one, and the strip's own right edge moved two
			// pixels between a pawn and a rook against a bar that is flush.
			const int ink = (JC_PIECE_INK_WIDTH[kind] * JC_CAPTURE_PX + JC_PIECE_PX / 2) /
			                JC_PIECE_PX;
			reach += ink;
			const int x = leader == JC_WHITE ? bar_x + JC_BAR_W - 2 - reach + ink / 2
			                                 : bar_x + reach - ink + ink / 2;
			reach += JC_CAPTURE_GAP;
			// The OPPONENT'S colour, which is the convention every chess
			// program keeps: a captured piece is shown beside the player who
			// took it, in the colour of the piece that was taken. Drawn in the
			// leader's own colour, "Black +1" with a black pawn beside it
			// reads to a chess player as "Black has LOST a pawn".
			jet_spr_ext_grid(jc_piece_cell(JC_PIECE(taken, kind), false, false), x,
			                 y + JC_CAPTURE_PX / 2, &format);
		}
	}
}

// Who is ahead: a bar that grows from its middle toward the leader, with the
// figure on the same line. One track colour — two read as a progress bar stuck
// at half — and the ends named on the label's own line, directly above the
// ends they name, so the bar needs no legend of its own.
static void paint_material(const JcView *view) {
	const int ink_y = JC_PANEL_Y + JC_PANEL_MATERIAL_ROW_Y;
	const int bar_x = JC_BLOCK_RIGHT - JC_BAR_W;
	const int middle = bar_x + JC_BAR_W / 2; // 120 either side of it
	const int bar_y = ink_y + (JC_CAP_TEXT - JC_BAR_H) / 2;

	// No BLACK / WHITE labels over the ends. They sat on the section label's
	// own line at 2.9 to 1 — two brightnesses of one label-sized style in a
	// single row, with no rank between them to justify it — and the line
	// directly below already says "Black +1". A legend that repeats the value
	// it labels is a legend nobody reads.
	paint_label("MATERIAL", JC_PANEL_Y + JC_PANEL_MATERIAL_Y);

	jet_rectfill(bar_x, bar_y, JC_BAR_W, JC_BAR_H, JC_PANEL_EDGE);
	const int lead = jc_match_material_lead(view->match) / 100;
	const int capped = lead > JC_BAR_MAX_PAWNS
	                       ? JC_BAR_MAX_PAWNS
	                       : (lead < -JC_BAR_MAX_PAWNS ? -JC_BAR_MAX_PAWNS : lead);
	// ROUNDED, and from the same expression either way. Truncating gave Black
	// 24 pixels for a one-pawn lead and White 23 — the same fact, two lengths.
	const int half = JC_BAR_W / 2;
	const int magnitude = capped < 0 ? -capped : capped;
	const int size = (magnitude * half * 2 + JC_BAR_MAX_PAWNS) / (JC_BAR_MAX_PAWNS * 2);
	const int span = capped < 0 ? -size : size;
	// THE ACCENT AT REST, and not the accent itself. A material lead is a
	// MEASUREMENT: not something the player may act on, not something that has
	// just happened. In full brass this bar was the largest block of colour in
	// the lower panel and it shouted down the one live thing down there, the
	// move at the foot of the scoresheet. In a neutral grey it stopped reading
	// as a bar at all — grey fill, near-grey tick, and the pair of them looked
	// like the thumb of a slider sitting at forty per cent. The accent's own
	// dim is the answer: plainly the same family, plainly quieter, and dark
	// enough that the tick across it can only be a tick.
	// The tick owns the middle column and NEITHER side fills it. Starting
	// White's fill on the tick let the tick paint over its first pixel, so the
	// same one-pawn lead came out 24 pixels one way and 23 the other.
	if (span > 0) {
		jet_rectfill(middle + 1, bar_y, span, JC_BAR_H, JC_BRASS_DIM);
	} else if (span < 0) {
		jet_rectfill(middle + span, bar_y, -span, JC_BAR_H, JC_BRASS_DIM);
	}
	// The tick stands proud of the track at both ends, or at one pixel flush
	// it is a thing nobody can find.
	// Not the reading colour. A one-pixel tick at 13.6 to one inside a track
	// that whispers at 1.3 is the loudest thing in the block, and all it has to
	// do is say where the middle is when nothing is filled.
	jet_rectfill(middle, bar_y - JC_BAR_TICK_OVERHANG, 1,
	             JC_BAR_H + 2 * JC_BAR_TICK_OVERHANG, JC_TEXT_DIM);

	JcLine line;
	jc_line_clear(&line);
	if (lead == 0) {
		jc_line_add(&line, "Even"); // NOT "Level": this panel has a level control
	} else {
		jc_line_add(&line, lead > 0 ? "White +" : "Black +");
		jc_line_add_number(&line, (unsigned)(lead > 0 ? lead : -lead));
	}
	jc_print_ink(line.text, JC_BLOCK_X, ink_y, JC_RUNG_VALUE, lead == 0 ? JC_TEXT_DIM : JC_TEXT);

	paint_captures(view, bar_x);
}

// The scoresheet: two column-pairs side by side, ten numbered moves in each.
// A printed scoresheet reads this way, it puts twenty moves on show instead of
// nine, and it uses the well's whole width — a single three-column list left a
// third of the box permanently empty.
static void paint_moves(const JcView *view) {
	const int x = JC_BLOCK_X;
	const int y = JC_PANEL_Y + JC_PANEL_MOVES_WELL_Y;
	paint_label("MOVES", JC_PANEL_Y + JC_PANEL_MOVES_Y);
	jet_rectfill(x, y, JC_BLOCK_W, JC_PANEL_MOVES_H, JC_PANEL_DEEP);
	jet_rect(x, y, JC_BLOCK_W, JC_PANEL_MOVES_H, JC_CONTAINER_EDGE);

	// The column heads, and the rule under them: without those the columns are
	// two ragged stacks of text rather than a table.
	for (int pair = 0; pair < JC_MOVE_COLUMNS; pair++) {
		const int at = x + JC_MOVE_GUTTER + pair * JC_MOVE_HALF;
		// A column head is FURNITURE and a move is the reading: the head was
		// the brighter of the two, which is the panel's own rule upside down.
		// It sits on its column's own x, with nothing added: both faces are
		// baked ones and both draw their ink at the pen.
		jc_print_ink("WHITE", at + JC_MOVE_WHITE_X, y + JC_MOVE_PAD, JC_RUNG_LABEL,
		             JC_TEXT_GUTTER);
		jc_print_ink("BLACK", at + JC_MOVE_BLACK_X, y + JC_MOVE_PAD, JC_RUNG_LABEL,
		             JC_TEXT_GUTTER);
	}
	jet_rectfill(x + 1, y + JC_MOVE_HEADER_H, JC_BLOCK_W - 2, 1, JC_PANEL_EDGE);

	const JcMatch *match = view->match;
	// Plies pair up into numbered moves; White's is the even ply of each pair.
	const int move_count = (match->ply_count + 1) / 2;
	if (move_count == 0) {
		// An empty list is a large dark rectangle, which looks like something
		// missing rather than something waiting. One quiet line says which.
		// Centred on the LIST — the area below the heads' rule — and not on
		// the box, which includes the heading band and put this nine pixels
		// high.
		const int list_y = y + JC_MOVE_HEADER_H;
		const int list_h = JC_PANEL_MOVES_H - JC_MOVE_HEADER_H;
		jc_print_ink_centre("The game will be written here", x + JC_BLOCK_W / 2,
		                    list_y + (list_h - JC_CAP_INSTRUMENT) / 2, JC_RUNG_MOVE, JC_TEXT_GUTTER, 0);
		return;
	}

	const int visible = JC_MOVE_ROWS * JC_MOVE_COLUMNS;
	int first = move_count - visible;
	if (first < 0) {
		first = 0;
	}
	for (int slot = 0; slot < visible && first + slot < move_count; slot++) {
		const int number = first + slot;
		const int at = x + JC_MOVE_GUTTER + (slot / JC_MOVE_ROWS) * JC_MOVE_HALF;
		const int ink_y = y + JC_MOVE_HEADER_H + JC_MOVE_PAD + (slot % JC_MOVE_ROWS) * JC_MOVE_ROW_H;

		JcLine line;
		jc_line_clear(&line);
		jc_line_add_number(&line, (unsigned)(number + 1));
		jc_line_add(&line, ".");
		jc_print_ink_right(line.text, at + JC_MOVE_NUMBER_RIGHT, ink_y, JC_RUNG_MOVE, JC_TEXT_GUTTER);

		// The move just played is the bright one and the rest step back. It
		// used to be the accent, but the accent already carries the selection,
		// the legal moves and the square under the pointer — a colour that
		// means five things means none, and brightness says "latest" perfectly
		// well in a list where every row is otherwise identical.
		const int white_ply = number * 2;
		jc_print_ink(match->played[white_ply].san, at + JC_MOVE_WHITE_X, ink_y, JC_RUNG_MOVE,
		             white_ply == match->ply_count - 1 ? JC_TEXT : JC_TEXT_GUTTER);

		const int black_ply = white_ply + 1;
		if (black_ply < match->ply_count) {
			jc_print_ink(match->played[black_ply].san, at + JC_MOVE_BLACK_X, ink_y, JC_RUNG_MOVE,
			             black_ply == match->ply_count - 1 ? JC_TEXT : JC_TEXT_GUTTER);
		}
	}
}

// ---------------------------------------------------------------------------
// The engine's own block: how strong it is, and what it is doing
// ---------------------------------------------------------------------------

void jc_panel_level_rect(int *x, int *y, int *w, int *h) {
	*w = JC_LEVEL_CHIP_W;
	*h = JC_LEVEL_CHIP_H;
	*x = JC_BLOCK_RIGHT - *w;
	// Centred on the label's own line, which is what makes it read as that
	// section's setting rather than as a fourth button.
	*y = JC_PANEL_Y + JC_PANEL_READOUT_Y + (JC_CAP_LABEL - *h) / 2;
}

// The opponent's strength: a SETTING, and settings are not actions, so it is
// not the accent's colour. The chevrons are anchored to the box and the name
// is centred between them, so the arrows stay put whichever level is showing.
//
// The NAME is a value and is set like one — the instrument face's eleven
// pixels of cap against the label face's nine. At the label's own size it was
// the only value in the panel that did not out-rank its label, and it read as
// part of the furniture rather than as the thing the block is about.
static void paint_level(const JcView *view, bool lit) {
	int x, y, w, h;
	jc_panel_level_rect(&x, &y, &w, &h);
	// A modal owns the panel, and this is part of the panel. It kept its live
	// border while the three buttons beside it went grey, which is a control
	// saying it can be pressed while the card in front of it says otherwise.
	const bool usable = view->hand->state != JC_HAND_PROMOTING;
	jet_rectfill(x, y, w, h, lit && usable ? JC_PANEL_EDGE : JC_PANEL_DEEP);
	jet_rect(x, y, w, h, usable ? JC_CONTROL_EDGE : JC_CONTAINER_EDGE);

	const int ink_y = y + (h - JC_CAP_LABEL) / 2;
	const uint8_t chevron = usable ? JC_TEXT_DIM : JC_TEXT_FAINT;
	jc_print_ink("<", x + JC_LEVEL_CHEVRON_INSET, ink_y, JC_RUNG_LABEL, chevron);
	jc_print_ink_right(">", x + w - JC_LEVEL_CHEVRON_INSET, ink_y, JC_RUNG_LABEL, chevron);
	jc_print_ink_centre(JC_LEVEL_TABLE[view->brain->level].name, x + w / 2,
	                    y + (h - JC_CAP_INSTRUMENT) / 2, JC_RUNG_LEVEL,
	                    !usable ? JC_TEXT_FAINT : lit ? JC_TEXT : JC_TEXT_DIM, JC_CONTROL_TRACK);
}

// What the engine is up to, in the voice of a footnote: how deep it has looked
// and how many positions that took.
static void readout_line(const JcView *view, JcLine *line) {
	jc_line_clear(line);
	if (view->brain->state == JC_BRAIN_WAITING) {
		jc_line_add(line, JC_LEVEL_TABLE[view->brain->level].blurb);
		return;
	}
	if (view->brain->from_book) {
		jc_line_add(line, "Playing from the book");
		return;
	}
	jc_line_add(line, "Thinking / depth ");
	jc_line_add_number(line, (unsigned)jc_brain_depth(view->brain));
	jc_line_add(line, " / ");
	const uint64_t nodes = jc_brain_nodes(view->brain);
	// Thousands past a thousand: the exact count of a search's positions is
	// not information, but its order of magnitude is.
	if (nodes < 1000) {
		jc_line_add_number(line, (unsigned)nodes);
	} else {
		jc_line_add_number(line, (unsigned)(nodes / 1000));
		jc_line_add(line, "k");
	}
	jc_line_add(line, " positions");
}

// The footnote, and — only while the engine is working — a thin bar under it
// that fills as the search deepens. It is drawn only then on purpose: a track
// with nothing in it is a two-pixel band that reads as a defect.
static void paint_readout(const JcView *view) {
	JcLine line;
	readout_line(view, &line);
	const int ink_y = JC_PANEL_Y + JC_PANEL_READOUT_LINE_Y;
	jc_print_ink(line.text, JC_BLOCK_X, ink_y, JC_RUNG_READOUT, JC_TEXT_GUTTER);

	if (view->brain->state == JC_BRAIN_THINKING) {
		const int wanted = JC_LEVEL_TABLE[view->brain->level].depth;
		int reached = jc_brain_depth(view->brain);
		if (reached > wanted) {
			reached = wanted;
		}
		const int track_y = ink_y + JC_CAP_INSTRUMENT + JC_READOUT_TRACK_DROP;
		jet_rectfill(JC_BLOCK_X, track_y, JC_BLOCK_W, JC_READOUT_TRACK_H, JC_PANEL_EDGE);
		jet_rectfill(JC_BLOCK_X, track_y, (JC_BLOCK_W * reached) / wanted, JC_READOUT_TRACK_H,
		             JC_BRASS);
	}
}

// ---------------------------------------------------------------------------
// The buttons
// ---------------------------------------------------------------------------

void jc_panel_chip_rect(int index, int *x, int *y, int *w, int *h) {
	const int usable = JC_BLOCK_W - (JC_CHIP_COUNT - 1) * JC_CHIP_GAP;
	const int width = usable / JC_CHIP_COUNT;
	const int spare = usable - width * JC_CHIP_COUNT;
	// THE SPARE PIXELS GO TO THE MIDDLE. 457 does not divide by three, and on
	// the last button the leftover made the row lean right by a pixel against
	// a column that lands on the same edge everywhere else. In the middle the
	// row stays symmetric about its own centre, which is what the eye checks.
	const int middle = JC_CHIP_COUNT / 2;
	*w = width + (index == middle ? spare : 0);
	*x = JC_BLOCK_X + index * (width + JC_CHIP_GAP) + (index > middle ? spare : 0);
	*y = JC_PANEL_Y + JC_PANEL_CHIPS_Y;
	*h = JC_CHIP_H;
}

int jc_panel_chip_at(int x, int y) {
	for (int index = 0; index < JC_CHIP_COUNT; index++) {
		int cx, cy, cw, ch;
		jc_panel_chip_rect(index, &cx, &cy, &cw, &ch);
		if (x >= cx && x < cx + cw && y >= cy && y < cy + ch) {
			return index;
		}
	}
	return -1;
}

// Three verbs, and only verbs.
static const char *const JC_CHIP_LABEL[JC_CHIP_COUNT] = {"NEW", "BACK", "FLIP"};

// One button. A button the player cannot use right now is drawn faint rather
// than hidden, so the row never changes shape.
static void paint_chip(const JcView *view, int index, bool lit) {
	int x, y, w, h;
	jc_panel_chip_rect(index, &x, &y, &w, &h);
	// A MODAL OWNS THE PANEL. While a pawn is waiting to be told what it
	// becomes, nothing else on screen may be pressed — a question with a live
	// "new game" button beside it is not a question.
	const bool usable = view->hand->state != JC_HAND_PROMOTING &&
	                    (index != JC_CHIP_BACK || view->match->ply_count > 0);
	// A finished game has exactly one sensible next action, so NEW takes a
	// bright border and says so; the rest of the time all three are equals.
	const bool primary = index == JC_CHIP_NEW && view->match->result != JC_RESULT_PLAYING;
	jet_rectfill(x, y, w, h, lit && usable ? JC_PANEL_EDGE : JC_PANEL_DEEP);
	// A disabled button KEEPS its border. Without one the row read as two
	// buttons and a gap rather than three with one of them off.
	jet_rect(x, y, w, h, primary ? JC_TEXT : usable ? JC_CONTROL_EDGE : JC_CONTAINER_EDGE);

	// The three controls used to carry the quietest text on the panel, which
	// made them look half-disabled next to body copy they should outrank.
	const uint8_t colour = usable ? JC_TEXT : JC_TEXT_FAINT;
	jc_print_ink_centre(JC_CHIP_LABEL[index], x + w / 2, y + (h - JC_CAP_STATEMENT) / 2,
	                    JC_RUNG_ACTION, colour, JC_CONTROL_TRACK);
}

// ---------------------------------------------------------------------------
// The panel as a whole
// ---------------------------------------------------------------------------

void jc_panel_paint(const JcView *view) {
	jet_rectfill(JC_PANEL_X, JC_PANEL_Y, JC_PANEL_W, JC_PANEL_H, JC_PANEL);
	jet_rect(JC_PANEL_X, JC_PANEL_Y, JC_PANEL_W, JC_PANEL_H, JC_CONTAINER_EDGE);
	paint_wordmark(view);
	paint_rule(JC_PANEL_Y + JC_PANEL_RULE_Y);
	paint_status(view);
	paint_opening(view);
	paint_material(view);
	paint_moves(view);
	paint_label("ENGINE", JC_PANEL_Y + JC_PANEL_READOUT_Y);
	paint_level(view, false);
	paint_readout(view);
	for (int index = 0; index < JC_CHIP_COUNT; index++) {
		paint_chip(view, index, false);
	}
}

void jc_panel_paint_overlay(const JcView *view, JetStill *still) {
	// The readout ticks over many times a second while the engine works, so it
	// belongs to the overlay rather than to the canvas: re-baking the whole
	// screen for a changing number would undo the point of having a canvas.
	if (view->brain->state != JC_BRAIN_WAITING) {
		const int y = JC_PANEL_Y + JC_PANEL_READOUT_LINE_Y - JC_INK_INSTRUMENT;
		const int height = JC_READOUT_BAND_H;
		jet_rectfill(JC_BLOCK_X, y, JC_BLOCK_W, height, JC_PANEL);
		paint_readout(view);
		jet_still_mark(still, JC_BLOCK_X, y, JC_BLOCK_W, height);
	}

	// Whatever the pointer is over, drawn lit over the unlit version the
	// canvas holds. Hovering is not in the fingerprint for the same reason the
	// board's hover ring is not: it follows the pointer, not the game.
	if (view->hand->hover_chip >= 0) {
		paint_chip(view, view->hand->hover_chip, true);
		int x, y, w, h;
		jc_panel_chip_rect(view->hand->hover_chip, &x, &y, &w, &h);
		jet_still_mark(still, x, y, w, h);
	}
	if (view->hand->hover_level) {
		paint_level(view, true);
		int x, y, w, h;
		jc_panel_level_rect(&x, &y, &w, &h);
		jet_still_mark(still, x, y, w, h);
	}
}
