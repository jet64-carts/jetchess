// carts/jetchess/ui/view.h
//
// What the screen is showing, gathered into one value so the painters take a
// single argument instead of five. Nothing here is state of its own: it is
// four pointers to the things that ARE state, plus the two small pieces of
// presentation — what the hand is doing and what is sliding — that belong to
// the interface rather than to the game.
#ifndef JETCHESS_UI_VIEW_H
#define JETCHESS_UI_VIEW_H

#include "engine/chess.h"
#include "game/brain.h"
#include "game/match.h"
#include "jet_still.h"

// What the player is doing with the pointer.
typedef enum {
	JC_HAND_EMPTY = 0, // nothing chosen
	JC_HAND_HOLDING,   // a piece is picked up, and its moves are shown
	JC_HAND_PROMOTING, // a pawn has arrived, and four pieces are on offer
} JcHandState;

typedef struct {
	JcHandState state;
	uint8_t from; // the square the held piece came from
	uint8_t to;   // where a promoting pawn is going
	JcMoveList options; // every legal move the held piece has

	bool dragging;  // the button has not been let go since the piece was taken
	int grab_x;     // where inside the piece the pointer took hold of it
	int grab_y;
	int pointer_x;  // where the pointer is, this frame
	int pointer_y;

	uint8_t hover;       // the square under the pointer, or JC_NO_SQUARE
	int hover_chip;      // the panel button under the pointer, or -1
	int hover_promotion; // the promotion choice under the pointer, or -1
	bool hover_level;    // the pointer is over the level control
	// The two settings that are the PLAYER's rather than the game's. They are
	// here beside the pointer's own business because they are what the player
	// has asked the interface for, and because they must survive a new game
	// and a take-back — which is what jc_hand_reset's argument is for.
	bool flipped;      // Black's side of the board is nearest the player
	bool showing_rate; // the frame-rate readout is on (ui/rate.h)
} JcHand;

// A piece travelling from one square to another under its own steam — what
// the engine's moves do, so the player can see what was played.
typedef struct {
	bool running;
	JcMove move;
	uint8_t piece;   // what is travelling (a promotion arrives as the pawn)
	float elapsed;
	float duration;
} JcSlide;

typedef struct {
	const JcMatch *match;
	const JcHand *hand;
	const JcBrain *brain;
	const JcSlide *slide;
} JcView;

// The square the overlay is carrying, and which the board must therefore NOT
// paint: the one in the hand, or the one in mid-slide. JC_NO_SQUARE when the
// board holds everything.
uint8_t jc_view_lifted_square(const JcView *view);

// Everything the canvas draws from, digested into one number. The compositor
// bakes the screen again when this moves and restores it when it does not
// (jet_still.h, law one), so a thing drawn into the canvas and left out of
// this digest is a thing that will not be redrawn when it changes.
uint64_t jc_view_fingerprint(const JcView *view);

#endif // JETCHESS_UI_VIEW_H
