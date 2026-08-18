// carts/jetchess/game/match.h
//
// A whole game: the position, everything played to reach it, and whether it is
// over. The engine (engine/) knows chess; a match knows a *game* of chess —
// the scoresheet, the opening's name, taking a move back, and the five ways a
// game can end.
//
// It owns no drawing and no sound. The interface asks it questions and tells
// it about moves; it answers and remembers.
#ifndef JETCHESS_GAME_MATCH_H
#define JETCHESS_GAME_MATCH_H

#include "engine/chess.h"

// The longest game the scoresheet holds. The official maximum before a game is
// drawn by rule is a little over five hundred moves; this holds every ply of
// one twice that length.
#define JC_MATCH_MAX_PLIES 1024

// How a game ended, or that it has not.
typedef enum {
	JC_RESULT_PLAYING = 0,
	JC_RESULT_CHECKMATE,      // the side to move is mated
	JC_RESULT_STALEMATE,      // no legal move, and not in check
	JC_RESULT_FIFTY_MOVES,    // fifty moves each with no pawn moved and nothing taken
	JC_RESULT_REPETITION,     // the same position, with the same moves available, three times
	JC_RESULT_INSUFFICIENT,   // neither side has enough left to mate with
} JcResult;

// One ply of the scoresheet.
typedef struct {
	JcMove move;
	JcUndo undo;
	char san[JC_SAN_LENGTH]; // how it is written, decided before it was played
} JcPlayed;

typedef struct {
	JcPosition position;
	JcPlayed played[JC_MATCH_MAX_PLIES];
	uint64_t key[JC_MATCH_MAX_PLIES + 1]; // the hash after each ply; [0] is the start
	int ply_count;

	JcResult result;
	const char *opening;   // the last opening name the game passed through, or NULL
	uint8_t human_colour;  // the side the player has
	uint32_t book_seed;    // this game's own thread through the opening book
} JcMatch;

// Start a new game. `human_colour` is JC_WHITE or JC_BLACK.
void jc_match_new(JcMatch *match, uint8_t human_colour, uint32_t seed);

// Play a move that must be legal in the current position. Writes its notation,
// updates the opening's name, and works out whether the game is now over.
void jc_match_play(JcMatch *match, JcMove move);

// Take back one ply. Returns false when there is nothing to take back.
bool jc_match_take_back(JcMatch *match);

// Every legal move that starts on `from`, for showing where a held piece may
// go. The list is empty when nothing of the player's stands there.
void jc_match_moves_from(const JcMatch *match, uint8_t from, JcMoveList *list);

// The one legal move from `from` to `to` that promotes to `promotion` (pass
// JC_EMPTY when the move is not a promotion), or a none-move when there is no
// such legal move.
JcMove jc_match_find_move(const JcMatch *match, uint8_t from, uint8_t to, uint8_t promotion);

// True when a move from `from` to `to` would be a promotion — asked before the
// player is offered the four pieces to choose between.
bool jc_match_is_promotion(const JcMatch *match, uint8_t from, uint8_t to);

// Whose turn it is, and whether that is the player.
static inline bool jc_match_humans_turn(const JcMatch *match) {
	return match->result == JC_RESULT_PLAYING && match->position.side == match->human_colour;
}

// The king's square, when it is in check — the square the board paints red.
// JC_NO_SQUARE when nobody is in check.
uint8_t jc_match_checked_king(const JcMatch *match);

// How much material one side is up, in pawns' worth. Positive means White.
int jc_match_material_lead(const JcMatch *match);

// What the leading side is UP, by kind — the pieces it has that its opponent
// no longer does, which is what "White is a bishop and a pawn ahead" means.
// Fills `counts` and answers which colour that is, or JC_COLOURS when the
// material is level.
uint8_t jc_match_advantage(const JcMatch *match, uint8_t counts[JC_KINDS]);

// A short sentence for the panel: "White to move", "Black is in check",
// "Checkmate — White wins", "Drawn by repetition".
const char *jc_match_status(const JcMatch *match);

// The result in the form a scoresheet ends with: "1-0", "0-1", "1/2-1/2", or
// NULL while the game is still on.
const char *jc_match_score_line(const JcMatch *match);

#endif // JETCHESS_GAME_MATCH_H
