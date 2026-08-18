// carts/jetchess/save.h
//
// Where the player left off, kept across runs.
//
// A GAME OF CHESS IS NOT A PICTURE OF A BOARD. It is a starting array and the
// moves played from it, and that is what is written down: the side the player
// has, the game's own thread through the opening book, and every move in
// order — plus the two choices that are the player's rather than the game's,
// the opponent's strength and which way round the board is.
//
// Everything else the screen shows is DERIVED, by replaying those moves into a
// fresh match: the pieces' squares, the scoresheet in the notation a chess book
// uses, the opening's name, whose turn it is, and whether the game is over and
// how. None of it is stored, so none of it can disagree with the rules — and a
// record that does disagree, because the rules moved under it between one
// version and the next, is caught move by move as it is replayed rather than
// setting a position up that chess would not allow.
//
// IT IS WRITTEN WHEN IT CHANGES, and noticing that is not the caller's job.
// jc_save_sync encodes the record once a tick and compares it with the bytes
// the file already holds; a difference is a write and everything else is a
// memcmp. That is the same discipline the screen is composited by (ui/view.h's
// fingerprint, jet_still.h's first law): a closed ring over the state, so a
// fact added to the record cannot be a fact that some new code path forgets to
// save. There is no jc_save_now() for a move, a take-back or a new game to
// remember to call, because there is nothing to remember.
//
// AND IT IS CHEAP ENOUGH TO DO THAT WITH. A game's worth of moves is a couple
// of hundred bytes laid into a buffer and compared with another, and the
// longest game the scoresheet holds is three kilobytes of it. The Cart
// Profiler, over six hundred ticks of a game in progress, does not find this
// among the cart's twenty-five hottest routines at all: the whole of _update —
// the player's hand, the engine's turn and this — measures 589 ns of the tick's
// 8,333,333.
//
// WHERE IT IS KEPT is the cartridge's own AppData folder, which every runtime
// gives it: the workstation, a desktop export, and a web export, where the host
// mirrors the folder into the browser's own storage and hands it back before
// _init runs. So the game comes back in all three.
#ifndef JETCHESS_SAVE_H
#define JETCHESS_SAVE_H

#include "game/brain.h"
#include "game/match.h"
#include "ui/view.h"

// The record's fixed head — magic, checksum, version, counts, seed and the two
// settings — and the three bytes each move takes after it: where it came from,
// where it went, and what a pawn became. Flags are a fact about the position
// rather than about the move, so they are not written: the move is looked up
// again in the position it is replayed into, which is what validates it.
#define JC_SAVE_PREAMBLE_BYTES 20
#define JC_SAVE_MOVE_BYTES 3
#define JC_SAVE_BYTES (JC_SAVE_PREAMBLE_BYTES + JC_SAVE_MOVE_BYTES * JC_MATCH_MAX_PLIES)

// The record, and what is known about the copy of it on disk. Both buffers are
// held here rather than on a stack: the longest game the scoresheet allows
// encodes to three kilobytes, and a three-kilobyte local in _update is exactly
// the shape this cart keeps its search and its scoresheet out of.
typedef struct {
	// How much of `image` is the record. Zero means the file's contents are not
	// known — no run has read or written it yet, or the last read refused it —
	// and the next sync therefore writes whatever the game is now.
	int length;
	uint8_t image[JC_SAVE_BYTES]; // what the file is believed to hold
	uint8_t draft[JC_SAVE_BYTES]; // this tick's encoding, weighed against it
} JcSave;

// Read the record back and put the cart where the player left it: the match is
// started again and every saved move replayed into it, the opponent's level is
// handed to the brain, and the hand is given back the two settings it keeps.
// Returns true when a game came back.
//
// Call it once, from _init, AFTER the cart has set a fresh game up: what the
// record holds replaces that game, and what it does not hold — the brain's
// tables, the WRAM it reserved — is left exactly as it was. A missing file, a
// file from another version, and a file that fails its checksum all answer
// false and change nothing, which is how a fresh cartridge boots.
bool jc_save_restore(JcSave *save, JcMatch *match, JcBrain *brain, JcHand *hand);

// Encode what is on the screen and write it if it differs from what the file
// holds. Call it once a tick, at the end of _update, after everything that
// could have changed the game has had its turn.
void jc_save_sync(JcSave *save, const JcView *view);

#endif // JETCHESS_SAVE_H
