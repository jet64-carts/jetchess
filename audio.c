// carts/jetchess/audio.c
//
// The sounds, and when each one plays (audio.h).
//
// A move can be several things at once — a capture that promotes and gives
// check — so the rule is: one sound for WHAT THE MOVE DID, and a second, laid
// over it, for what it did to the king. Two short sounds on two voices read as
// one event with two facts in it, which is what a move like that is.
#include "audio.h"

#include "jet64.h"
#include "jet_audio.h"
#include "sfx.h"

void jc_audio_move_played(const JcMatch *match) {
	if (match->ply_count == 0) {
		return;
	}
	const JcMove move = match->played[match->ply_count - 1].move;

	// What the move did, in order of how much it changes the board.
	if (move.flags & JC_MOVE_PROMOTION) {
		jet_sfx_play(JC_SFX_PROMOTE);
	} else if (move.flags & JC_MOVE_CASTLE) {
		jet_sfx_play(JC_SFX_CASTLE);
	} else if (move.flags & JC_MOVE_CAPTURE) {
		jet_sfx_play(JC_SFX_CAPTURE);
	} else {
		jet_sfx_play(JC_SFX_MOVE);
	}

	// ...and what it did to the king. The end of the game outranks a check,
	// because a checkmate is a check nobody needs telling about twice.
	if (match->result != JC_RESULT_PLAYING) {
		jet_sfx_play(JC_SFX_GAME_OVER);
	} else if (jc_in_check(&match->position)) {
		jet_sfx_play(JC_SFX_CHECK);
	}
}

void jc_audio_click(void) {
	jet_sfx_play(JC_SFX_CLICK);
}

void jc_audio_refuse(void) {
	jet_sfx_play(JC_SFX_REFUSE);
}
