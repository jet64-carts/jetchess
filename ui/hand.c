// carts/jetchess/ui/hand.c
//
// Reading the player (hand.h).
#include "hand.h"

#include "config.h"
#include "jet64.h"
#include "jet_input.h"
#include "ui/board.h"
#include "ui/panel.h"

void jc_hand_reset(JcHand *hand, bool keep_settings) {
	// The player's own two settings, lifted out and put back: everything else
	// about a hand is about the move being made, and a move that is over
	// leaves nothing behind.
	const bool flipped = keep_settings && hand->flipped;
	const bool showing_rate = keep_settings && hand->showing_rate;
	memset(hand, 0, sizeof *hand);
	hand->from = JC_NO_SQUARE;
	hand->to = JC_NO_SQUARE;
	hand->hover = JC_NO_SQUARE;
	hand->hover_chip = -1;
	hand->hover_promotion = -1;
	hand->hover_level = false;
	hand->flipped = flipped;
	hand->showing_rate = showing_rate;
}

// The move the held piece would make by arriving at `square`, or a none-move
// when it may not go there. A promotion answers with its queen, because the
// four choices are the same move until the player says otherwise.
static JcMove option_to(const JcHand *hand, uint8_t square) {
	for (int i = 0; i < hand->options.count; i++) {
		const JcMove move = hand->options.moves[i];
		if (move.to != square) {
			continue;
		}
		if ((move.flags & JC_MOVE_PROMOTION) && move.promotion != JC_QUEEN) {
			continue;
		}
		return move;
	}
	return jc_move_none();
}

// Pick a piece up: remember where it came from, where in it the pointer took
// hold, and everywhere it may go.
static void take_up(JcHand *hand, const JcMatch *match, uint8_t square) {
	int piece_x, piece_y;
	jc_board_piece_origin(square, hand->flipped, &piece_x, &piece_y);
	hand->state = JC_HAND_HOLDING;
	hand->from = square;
	hand->dragging = true;
	hand->grab_x = hand->pointer_x - piece_x;
	hand->grab_y = hand->pointer_y - piece_y;
	jc_match_moves_from(match, square, &hand->options);
}

// Where a carried piece is really being aimed: its own middle, not the
// pointer. Dropping a piece is about where the PIECE is.
static uint8_t carried_target(const JcHand *hand) {
	const int x = hand->pointer_x - hand->grab_x + JC_PIECE_PX / 2;
	const int y = hand->pointer_y - hand->grab_y + JC_PIECE_PX / 2;
	return jc_board_square_at(x, y, hand->flipped);
}

// A move the player has completed, as an intent — stepping into the promotion
// choice instead when the move needs one.
static JcIntent settle_on(JcHand *hand, const JcMatch *match, uint8_t square) {
	JcIntent intent = {JC_INTENT_NONE, jc_move_none()};
	const JcMove move = option_to(hand, square);
	if (jc_move_is_none(move)) {
		return intent;
	}
	if (move.flags & JC_MOVE_PROMOTION) {
		hand->state = JC_HAND_PROMOTING;
		hand->to = square;
		hand->dragging = false;
		return intent;
	}
	const uint8_t from = hand->from;
	jc_hand_reset(hand, true);
	intent.kind = JC_INTENT_MOVE;
	intent.move = jc_match_find_move(match, from, square, JC_EMPTY);
	return intent;
}

// The keys. The first four each have a control in the panel that does the same
// thing, so nobody has to learn them, and anybody who wants to need not reach
// for the mouse.
//
// R is the one that does not, and deliberately: the frame rate is a
// DIAGNOSTIC, not a thing you do to a game of chess, and the row along the
// bottom of the panel is three verbs by design (config.h). A control for it
// would put the machine's own health in front of a player who came to play.
//
// None of these is TAB, which the workstation claims for itself (claude.md
// note #5).
static JcIntentKind key_intent(void) {
	if (jet_key_pressed(JET_KEY_N)) {
		return JC_INTENT_NEW_GAME;
	}
	if (jet_key_pressed(JET_KEY_U)) {
		return JC_INTENT_TAKE_BACK;
	}
	if (jet_key_pressed(JET_KEY_F)) {
		return JC_INTENT_FLIP;
	}
	if (jet_key_pressed(JET_KEY_L)) {
		return JC_INTENT_LEVEL;
	}
	if (jet_key_pressed(JET_KEY_R)) {
		return JC_INTENT_RATE;
	}
	return JC_INTENT_NONE;
}

static const JcIntentKind JC_CHIP_INTENT[JC_CHIP_COUNT] = {
	JC_INTENT_NEW_GAME, JC_INTENT_TAKE_BACK, JC_INTENT_FLIP};

// Is the pointer over the level control, which lives at the end of the ENGINE
// block's label line rather than in the row of verbs below?
static bool over_level(int x, int y) {
	int lx, ly, lw, lh;
	jc_panel_level_rect(&lx, &ly, &lw, &lh);
	return x >= lx && x < lx + lw && y >= ly && y < ly + lh;
}

JcIntent jc_hand_update(JcHand *hand, const JcMatch *match, const JcView *view) {
	JcIntent intent = {JC_INTENT_NONE, jc_move_none()};

	hand->pointer_x = (int)jet_mouse_x();
	hand->pointer_y = (int)jet_mouse_y();
	hand->hover = jc_board_square_at(hand->pointer_x, hand->pointer_y, hand->flipped);
	hand->hover_chip = jc_panel_chip_at(hand->pointer_x, hand->pointer_y);
	hand->hover_level = over_level(hand->pointer_x, hand->pointer_y);
	hand->hover_promotion = -1;

	const bool pressed = jet_mouse_pressed(MOUSE_BTN_LEFT);
	const bool released = jet_mouse_released(MOUSE_BTN_LEFT);
	const bool cancelled = jet_mouse_pressed(MOUSE_BTN_RIGHT);

	// ---- the panel's controls, and the keys that do the same --------------
	const JcIntentKind from_key = key_intent();
	if (from_key != JC_INTENT_NONE) {
		intent.kind = from_key;
		return intent;
	}
	if (pressed && hand->hover_chip >= 0) {
		intent.kind = JC_CHIP_INTENT[hand->hover_chip];
		return intent;
	}
	if (pressed && hand->hover_level) {
		intent.kind = JC_INTENT_LEVEL;
		return intent;
	}

	// ---- choosing what a pawn becomes -------------------------------------
	if (hand->state == JC_HAND_PROMOTING) {
		for (int index = 0; index < JC_PROMOTE_CHOICES; index++) {
			int x, y, size;
			if (!jc_board_promotion_slot(view, index, &x, &y, &size)) {
				break;
			}
			if (hand->pointer_x >= x && hand->pointer_x < x + size && hand->pointer_y >= y &&
			    hand->pointer_y < y + size) {
				hand->hover_promotion = index;
			}
		}
		if (pressed && hand->hover_promotion >= 0) {
			const uint8_t from = hand->from, to = hand->to;
			const uint8_t kind = jc_board_promotion_kind(hand->hover_promotion);
			jc_hand_reset(hand, true);
			intent.kind = JC_INTENT_MOVE;
			intent.move = jc_match_find_move(match, from, to, kind);
		} else if (pressed || cancelled) {
			// Anywhere but a choice: the question goes away and the pawn stays
			// on the seventh. The card is exactly its four board squares, so
			// there is no frame to mis-click.
			jc_hand_reset(hand, true);
		}
		return intent;
	}

	// ---- nothing else happens while it is not the player's turn -----------
	if (!jc_match_humans_turn(match)) {
		if (pressed && hand->hover != JC_NO_SQUARE) {
			intent.kind = JC_INTENT_REFUSED;
		}
		jc_hand_reset(hand, true);
		return intent;
	}

	if (cancelled) {
		jc_hand_reset(hand, true);
		return intent;
	}

	// ---- picking a piece up, and putting it down --------------------------
	if (pressed && hand->hover != JC_NO_SQUARE) {
		if (hand->state == JC_HAND_HOLDING) {
			const JcIntent played = settle_on(hand, match, hand->hover);
			if (played.kind != JC_INTENT_NONE || hand->state == JC_HAND_PROMOTING) {
				return played;
			}
		}
		const uint8_t piece = match->position.board[hand->hover];
		if (piece != JC_EMPTY && JC_COLOUR_OF(piece) == match->position.side) {
			take_up(hand, match, hand->hover);
		} else {
			// A click on a square the held piece cannot reach puts it down
			// again, and says so if the player was clearly trying to move.
			if (hand->state == JC_HAND_HOLDING) {
				intent.kind = JC_INTENT_REFUSED;
			}
			jc_hand_reset(hand, true);
		}
		return intent;
	}

	if (released && hand->state == JC_HAND_HOLDING && hand->dragging) {
		const uint8_t target = carried_target(hand);
		if (target == hand->from || target == JC_NO_SQUARE) {
			// Let go where it started: the piece stays up, and the player can
			// now click where it should go instead.
			hand->dragging = false;
			return intent;
		}
		const JcIntent played = settle_on(hand, match, target);
		if (played.kind == JC_INTENT_NONE && hand->state != JC_HAND_PROMOTING) {
			intent.kind = JC_INTENT_REFUSED;
			jc_hand_reset(hand, true);
			return intent;
		}
		return played;
	}

	return intent;
}
