// carts/jetchess/ui/view.c
//
// Two questions about what the screen is showing (view.h).
//
// THE FINGERPRINT is the more important of the two, and it is the reason the
// cart is fast. It digests every piece of state the canvas draws from into one
// 64-bit number; the compositor bakes when that number moves and restores when
// it does not (jet_still.h, law one). So the list below is a contract: a thing
// drawn into the canvas and left out of this function is a thing that will not
// be redrawn when it changes. Anything that moves EVERY frame — the pointer,
// the piece in the hand, a piece in mid-slide, the engine's readout — is
// deliberately absent, because it belongs to the overlay instead.
#include "view.h"

#include "config.h"

uint8_t jc_view_lifted_square(const JcView *view) {
	// A move is played the moment it is decided, and the slide is only the
	// picture catching up — so the travelling piece is already standing on the
	// square it is travelling TO, and that is the square the board must leave
	// empty while the overlay carries it.
	if (view->slide->running) {
		return view->slide->move.to;
	}
	if (view->hand->state == JC_HAND_HOLDING) {
		return view->hand->from;
	}
	return JC_NO_SQUARE;
}

uint64_t jc_view_fingerprint(const JcView *view) {
	const JcMatch *match = view->match;
	const JcHand *hand = view->hand;

	// The board: every piece, whose turn it is, the rights and the en-passant
	// square, all of which the position's own hash already carries.
	uint64_t digest = jet_still_fold(0, match->position.key);
	digest = jet_still_fold(digest, (uint64_t)match->result);

	// The scoresheet, and the last move the board picks out. The count is
	// enough for both: the same count over the same position is the same list.
	digest = jet_still_fold(digest, (uint64_t)match->ply_count);
	digest = jet_still_fold(digest, (uint64_t)(uintptr_t)match->opening);

	// The hand, which decides which square is picked out, which piece the
	// board must leave to the overlay, and which moves are shown as dots.
	digest = jet_still_fold(digest, (uint64_t)hand->state);
	digest = jet_still_fold(digest, (uint64_t)hand->from);
	digest = jet_still_fold(digest, (uint64_t)hand->to);
	digest = jet_still_fold(digest, (uint64_t)hand->flipped);

	// The square under the pointer, but ONLY while a piece is held — because
	// that is the one case where the canvas draws differently for it: the dot
	// beneath the held piece is left out (ui/board.c). Folding in the hover at
	// all other times would bake the whole screen every time the pointer
	// crossed a square, which is a hundred bakes a second for nothing.
	digest = jet_still_fold(digest,
	              (uint64_t)(hand->state == JC_HAND_HOLDING ? hand->hover : JC_NO_SQUARE));

	// The slide, for the square it hides. Its POSITION is left out on purpose:
	// it moves every frame, and the overlay is what draws it.
	digest = jet_still_fold(digest, (uint64_t)view->slide->running);
	digest = jet_still_fold(digest, (uint64_t)view->slide->move.to);

	// The panel's readout is baked in its resting state and painted over while
	// the engine works, so the canvas has to be told when that changes.
	digest = jet_still_fold(digest, (uint64_t)view->brain->state);
	digest = jet_still_fold(digest, (uint64_t)view->brain->from_book);
	digest = jet_still_fold(digest, (uint64_t)view->brain->level);
	return digest;
}
