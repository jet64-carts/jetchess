// carts/jetchess/ui/hand.h
//
// The player's hand: what the mouse and the keyboard are asking for.
//
// This is the only file that reads an input device, and it answers in the
// game's own words rather than in the machine's — "play this move", "take one
// back" — so nothing downstream has to know a button from a key. The two ways
// of moving a piece that every chess program supports both live here:
//
//   PICK UP AND CARRY. Press on a piece and it comes up under the pointer and
//   travels with it; let go over a square and it goes there. Let go where it
//   started and it stays up, which is how a drag turns into the other way:
//
//   CLICK AND CLICK. Click a piece, then click where it should go.
//
// They are the same state machine. A piece is either in the hand or it is
// not; whether the button is still down only decides whether the piece follows
// the pointer.
#ifndef JETCHESS_UI_HAND_H
#define JETCHESS_UI_HAND_H

#include "game/match.h"
#include "ui/view.h"

// What the player has asked for this tick.
typedef enum {
	JC_INTENT_NONE = 0,
	JC_INTENT_MOVE,      // `move` is legal in the current position: play it
	JC_INTENT_NEW_GAME,
	JC_INTENT_TAKE_BACK,
	JC_INTENT_FLIP,
	JC_INTENT_LEVEL,
	JC_INTENT_RATE,      // show the frame rate, or stop showing it (ui/rate.h)
	JC_INTENT_REFUSED,   // something was tried that the rules do not allow
} JcIntentKind;

typedef struct {
	JcIntentKind kind;
	JcMove move;
} JcIntent;

// Empty the hand. `keep_settings` leaves the player's own two — which way
// round the board is, and whether the frame rate is showing — as they were,
// which is what a new game wants and what a take-back wants. Only the boot
// clears them.
void jc_hand_reset(JcHand *hand, bool keep_settings);

// Read the devices and answer with what the player wants. Called once a tick.
JcIntent jc_hand_update(JcHand *hand, const JcMatch *match, const JcView *view);

#endif // JETCHESS_UI_HAND_H
