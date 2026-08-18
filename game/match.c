// carts/jetchess/game/match.c
//
// A game of chess, as opposed to the rules of chess (match.h).
//
// Two things here are worth reading twice.
//
// THE NOTATION IS DECIDED BEFORE THE MOVE IS PLAYED. "Nbd2" is only "Nbd2"
// because another knight could also have gone to d2 — a fact about the
// position *before* the move. So the scoresheet is written on the way in, not
// reconstructed afterwards, and taking a move back simply forgets it.
//
// THE OPENING'S NAME STICKS. The book names positions, not games. A game that
// has left the book is still the opening it came from, so the last name found
// stays until another replaces it — which is exactly how a commentator speaks.
#include "match.h"


#include "engine/book.h"

void jc_match_new(JcMatch *match, uint8_t human_colour, uint32_t seed) {
	memset(match, 0, sizeof *match);
	jc_position_start(&match->position);
	match->key[0] = match->position.key;
	match->human_colour = human_colour;
	// An odd seed: the generator the book draws with never reaches zero, and
	// zero would leave it stuck there.
	match->book_seed = seed | 1u;
	match->opening = NULL;
	match->result = JC_RESULT_PLAYING;
}

// How many times the position now on the board has stood before, this one
// included. Only positions since the last pawn move or capture can match, and
// only every second one, because the side to move has to agree.
static int times_repeated(const JcMatch *match) {
	int seen = 1;
	const int reach = match->position.halfmove < match->ply_count ? match->position.halfmove
	                                                              : match->ply_count;
	for (int back = 2; back <= reach; back += 2) {
		if (match->key[match->ply_count - back] == match->position.key) {
			seen++;
		}
	}
	return seen;
}

// The five ways a game ends, in the order the rules apply them: a position
// with no legal move is settled first, because a checkmate on the hundredth
// quiet move is still a checkmate.
static JcResult settle(const JcMatch *match) {
	JcMoveList legal;
	jc_generate_legal(&match->position, &legal);
	if (legal.count == 0) {
		return jc_in_check(&match->position) ? JC_RESULT_CHECKMATE : JC_RESULT_STALEMATE;
	}
	if (jc_is_insufficient_material(&match->position)) {
		return JC_RESULT_INSUFFICIENT;
	}
	if (times_repeated(match) >= 3) {
		return JC_RESULT_REPETITION;
	}
	if (match->position.halfmove >= 100) {
		return JC_RESULT_FIFTY_MOVES;
	}
	return JC_RESULT_PLAYING;
}

void jc_match_play(JcMatch *match, JcMove move) {
	if (match->ply_count >= JC_MATCH_MAX_PLIES) {
		return;
	}
	JcPlayed *entry = &match->played[match->ply_count];
	entry->move = move;
	jc_move_to_san(&match->position, move, entry->san);
	jc_make(&match->position, move, &entry->undo);
	match->ply_count++;
	match->key[match->ply_count] = match->position.key;

	const char *named = jc_book_name(match->position.key);
	if (named) {
		match->opening = named;
	}
	match->result = settle(match);
}

bool jc_match_take_back(JcMatch *match) {
	if (match->ply_count == 0) {
		return false;
	}
	match->ply_count--;
	const JcPlayed *entry = &match->played[match->ply_count];
	jc_unmake(&match->position, entry->move, &entry->undo);
	match->result = JC_RESULT_PLAYING;

	// The opening's name is re-derived rather than remembered, because the
	// name that stuck may have belonged to the move just taken back.
	match->opening = NULL;
	for (int ply = 0; ply <= match->ply_count; ply++) {
		const char *named = jc_book_name(match->key[ply]);
		if (named) {
			match->opening = named;
		}
	}
	return true;
}

void jc_match_moves_from(const JcMatch *match, uint8_t from, JcMoveList *list) {
	JcMoveList legal;
	jc_generate_legal(&match->position, &legal);
	list->count = 0;
	for (int i = 0; i < legal.count; i++) {
		if (legal.moves[i].from == from) {
			list->moves[list->count] = legal.moves[i];
			list->scores[list->count] = 0;
			list->count++;
		}
	}
}

JcMove jc_match_find_move(const JcMatch *match, uint8_t from, uint8_t to, uint8_t promotion) {
	JcMoveList legal;
	jc_generate_legal(&match->position, &legal);
	for (int i = 0; i < legal.count; i++) {
		const JcMove move = legal.moves[i];
		if (move.from == from && move.to == to && move.promotion == promotion) {
			return move;
		}
	}
	return jc_move_none();
}

bool jc_match_is_promotion(const JcMatch *match, uint8_t from, uint8_t to) {
	JcMoveList legal;
	jc_generate_legal(&match->position, &legal);
	for (int i = 0; i < legal.count; i++) {
		if (legal.moves[i].from == from && legal.moves[i].to == to &&
		    (legal.moves[i].flags & JC_MOVE_PROMOTION)) {
			return true;
		}
	}
	return false;
}

uint8_t jc_match_checked_king(const JcMatch *match) {
	return jc_in_check(&match->position) ? match->position.king[match->position.side]
	                                     : JC_NO_SQUARE;
}

int jc_match_material_lead(const JcMatch *match) {
	int lead = 0;
	for (uint8_t colour = 0; colour < JC_COLOURS; colour++) {
		const uint8_t count = match->position.roster_count[colour];
		int side_total = 0;
		for (uint8_t slot = 0; slot < count; slot++) {
			side_total += jc_piece_value(JC_KIND(match->position.board[match->position.roster[colour][slot]]));
		}
		lead += colour == JC_WHITE ? side_total : -side_total;
	}
	return lead;
}

uint8_t jc_match_advantage(const JcMatch *match, uint8_t counts[JC_KINDS]) {
	// What each side still HAS, by kind. What it has lost is the difference
	// between the two sides, and that is the honest thing to show: a board
	// where both sides have taken a rook is a board where neither is up a
	// rook, and a row of eight captured pieces says less than the two that
	// are not cancelled out by anything.
	int have[JC_COLOURS][JC_KINDS] = {{0}};
	for (uint8_t colour = 0; colour < JC_COLOURS; colour++) {
		for (uint8_t slot = 0; slot < match->position.roster_count[colour]; slot++) {
			const uint8_t piece = match->position.board[match->position.roster[colour][slot]];
			have[colour][JC_KIND(piece)]++;
		}
	}
	for (uint8_t kind = 0; kind < JC_KINDS; kind++) {
		counts[kind] = 0;
	}
	const int lead = jc_match_material_lead(match);
	if (lead == 0) {
		return JC_COLOURS; // nobody
	}
	const uint8_t leader = lead > 0 ? JC_WHITE : JC_BLACK;
	const uint8_t other = leader == JC_WHITE ? JC_BLACK : JC_WHITE;
	for (uint8_t kind = 0; kind < JC_KINDS; kind++) {
		const int spare = have[leader][kind] - have[other][kind];
		counts[kind] = spare > 0 ? (uint8_t)spare : 0;
	}
	return leader;
}

const char *jc_match_status(const JcMatch *match) {
	const bool white_to_move = match->position.side == JC_WHITE;
	// Sentence case, no full stops, and plain ASCII: these are captions rather
	// than prose, and the Font ROM holds the printable ASCII range and nothing
	// else — an em dash would be baked as the replacement box.
	//
	// AND THEY ARE SHORT. The status line is set in the display face at twenty
	// pixels, which is a monospaced face: twenty-three characters is the whole
	// width of the panel's block and the twenty-fourth is off the edge. The
	// geometry gate measures every one of these against that width, so a
	// sentence that has grown too long fails the build rather than the eye.
	switch (match->result) {
	case JC_RESULT_CHECKMATE:
		return white_to_move ? "Checkmate - Black wins" : "Checkmate - White wins";
	case JC_RESULT_STALEMATE:
		return "Stalemate";
	case JC_RESULT_FIFTY_MOVES:
		return "Drawn - fifty-move rule";
	case JC_RESULT_REPETITION:
		return "Drawn - repetition";
	case JC_RESULT_INSUFFICIENT:
		return "Drawn - no mating force";
	default:
		break;
	}
	if (jc_in_check(&match->position)) {
		return white_to_move ? "White is in check" : "Black is in check";
	}
	return white_to_move ? "White to move" : "Black to move";
}

const char *jc_match_score_line(const JcMatch *match) {
	switch (match->result) {
	case JC_RESULT_CHECKMATE:
		return match->position.side == JC_WHITE ? "0-1" : "1-0";
	case JC_RESULT_STALEMATE:
	case JC_RESULT_FIFTY_MOVES:
	case JC_RESULT_REPETITION:
	case JC_RESULT_INSUFFICIENT:
		return "1/2-1/2";
	default:
		return NULL;
	}
}
