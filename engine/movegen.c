// carts/jetchess/engine/movegen.c
//
// Every move a position offers.
//
// The generator is *pseudo-legal*: it obeys every rule about how pieces move
// and only skips one — that a move must not leave your own king in check. The
// search settles that question by playing the move and asking (jc_make_if_legal),
// which costs almost nothing because it has to play the move anyway. Only the
// interface and the rules need the fully filtered list, and they ask rarely.
//
// Castling is the exception: it is generated fully legal. The king may not
// start in check, pass through an attacked square, or land on one, and those
// three questions are cheapest to ask here, once, while the rook and the empty
// squares are already to hand.
//
// The roster (board.c) means each of these walks sixteen pieces rather than
// sixty-four squares, and the 0x88 layout means every "did I fall off the
// edge?" test is a single AND.
#include "chess.h"

#include "engine/internal.h"

// The four kinds a pawn may become. Queen first, because it is the one anybody
// wants and the search's move ordering looks at the front of the list.
static const uint8_t JC_PROMOTION_KINDS[4] = {JC_QUEEN, JC_ROOK, JC_BISHOP, JC_KNIGHT};
#define JC_PROMOTION_CHOICES 4

// ---------------------------------------------------------------------------
// Putting a move in the list
// ---------------------------------------------------------------------------

static void emit(JcMoveList *list, uint8_t from, uint8_t to, uint8_t promotion, uint8_t flags) {
	JcMove *move = &list->moves[list->count];
	move->from = from;
	move->to = to;
	move->promotion = promotion;
	move->flags = flags;
	list->scores[list->count] = 0;
	list->count++;
}

// A pawn arriving on the last rank is four moves, not one.
static void emit_pawn(JcMoveList *list, uint8_t from, uint8_t to, uint8_t flags, uint8_t colour) {
	if (JC_RANK_OF(to) != JC_PROMOTION_RANK(colour)) {
		emit(list, from, to, JC_EMPTY, flags);
		return;
	}
	for (int choice = 0; choice < JC_PROMOTION_CHOICES; choice++) {
		emit(list, from, to, JC_PROMOTION_KINDS[choice], (uint8_t)(flags | JC_MOVE_PROMOTION));
	}
}

// ---------------------------------------------------------------------------
// One piece's moves
// ---------------------------------------------------------------------------
//
// `quiet` says whether moves that capture nothing are wanted. The quiescence
// search wants captures only, and passing that down rather than filtering
// afterwards means the moves it does not want are never built.

static void leaper_moves(const JcPosition *position, JcMoveList *list, uint8_t from,
                         const int8_t *steps, int step_count, uint8_t colour, bool quiet) {
	for (int i = 0; i < step_count; i++) {
		const int to = (int)from + steps[i];
		if (!JC_ON_BOARD(to)) {
			continue;
		}
		const uint8_t target = position->board[to];
		if (target == JC_EMPTY) {
			if (quiet) {
				emit(list, from, (uint8_t)to, JC_EMPTY, 0);
			}
		} else if (JC_COLOUR_OF(target) != colour) {
			emit(list, from, (uint8_t)to, JC_EMPTY, JC_MOVE_CAPTURE);
		}
	}
}

static void slider_moves(const JcPosition *position, JcMoveList *list, uint8_t from,
                         int first_direction, int direction_count, uint8_t colour, bool quiet) {
	for (int i = 0; i < direction_count; i++) {
		const int step = JC_DIRECTIONS[first_direction + i];
		for (int to = (int)from + step; JC_ON_BOARD(to); to += step) {
			const uint8_t target = position->board[to];
			if (target == JC_EMPTY) {
				if (quiet) {
					emit(list, from, (uint8_t)to, JC_EMPTY, 0);
				}
				continue;
			}
			if (JC_COLOUR_OF(target) != colour) {
				emit(list, from, (uint8_t)to, JC_EMPTY, JC_MOVE_CAPTURE);
			}
			break; // a piece, whoever's, stops the slide
		}
	}
}

static void pawn_moves(const JcPosition *position, JcMoveList *list, uint8_t from,
                       uint8_t colour, bool quiet) {
	const int forward = JC_FORWARD(colour);

	if (quiet) {
		const int ahead = (int)from + forward;
		if (JC_ON_BOARD(ahead) && position->board[ahead] == JC_EMPTY) {
			emit_pawn(list, from, (uint8_t)ahead, 0, colour);
			// The two-square opening push, only from the pawn's own rank and
			// only when both squares ahead are clear.
			const int twice = ahead + forward;
			if (JC_RANK_OF(from) == JC_PAWN_RANK(colour) && position->board[twice] == JC_EMPTY) {
				emit(list, from, (uint8_t)twice, JC_EMPTY, JC_MOVE_DOUBLE_PUSH);
			}
		}
	}

	for (int side_step = -1; side_step <= 1; side_step += 2) {
		const int to = (int)from + forward + side_step;
		if (!JC_ON_BOARD(to)) {
			continue;
		}
		const uint8_t target = position->board[to];
		if (target != JC_EMPTY && JC_COLOUR_OF(target) != colour) {
			emit_pawn(list, from, (uint8_t)to, JC_MOVE_CAPTURE, colour);
		} else if (target == JC_EMPTY && (uint8_t)to == position->en_passant) {
			// En passant: the target square is empty, and the pawn taken
			// stands beside it. The position only records this square when
			// the capture is really available (board.c), so no further test
			// is needed here.
			emit(list, from, (uint8_t)to, JC_EMPTY, JC_MOVE_CAPTURE | JC_MOVE_EN_PASSANT);
		}
	}

	// A pawn promoting is worth looking at even when it captures nothing, so
	// the capture-only generator still wants the push that promotes.
	if (!quiet) {
		const int ahead = (int)from + forward;
		if (JC_ON_BOARD(ahead) && position->board[ahead] == JC_EMPTY &&
		    JC_RANK_OF(ahead) == JC_PROMOTION_RANK(colour)) {
			emit_pawn(list, from, (uint8_t)ahead, 0, colour);
		}
	}
}

// ---------------------------------------------------------------------------
// Castling
// ---------------------------------------------------------------------------

// One castle, if the rights, the empty squares and the three safety questions
// all allow it. `king_file` and `rook_file` are where the two start; the king
// always finishes two files toward the rook.
static void castle_moves(const JcPosition *position, JcMoveList *list, uint8_t colour, uint8_t rights) {
	const int rank = colour == JC_WHITE ? 0 : 7;
	const uint8_t king_from = JC_SQUARE(4, rank);
	const uint8_t enemy = JC_OPPONENT(colour);
	if (position->board[king_from] != JC_PIECE(colour, JC_KING)) {
		return;
	}
	// Short side first, then long: {the right, the rook's file, the king's
	// destination file, the first empty file, the last empty file}.
	static const struct {
		uint8_t side_bit; // 0 for the short side, 1 for the long one
		int8_t rook_file;
		int8_t king_to_file;
		int8_t empty_from;
		int8_t empty_to;
	} SIDES[2] = {
		{0, 7, 6, 5, 6},
		{1, 0, 2, 1, 3},
	};
	for (int side = 0; side < 2; side++) {
		const uint8_t right = (uint8_t)(1 << ((colour * 2) + SIDES[side].side_bit));
		if ((rights & right) == 0) {
			continue;
		}
		if (position->board[JC_SQUARE(SIDES[side].rook_file, rank)] != JC_PIECE(colour, JC_ROOK)) {
			continue;
		}
		bool clear = true;
		for (int file = SIDES[side].empty_from; file <= SIDES[side].empty_to && clear; file++) {
			clear = position->board[JC_SQUARE(file, rank)] == JC_EMPTY;
		}
		if (!clear) {
			continue;
		}
		// The king may not start in check, cross an attacked square, or land
		// on one. The square it crosses is always the one between.
		const uint8_t king_to = JC_SQUARE(SIDES[side].king_to_file, rank);
		const uint8_t crossed = JC_SQUARE((4 + SIDES[side].king_to_file) / 2, rank);
		if (jc_is_attacked(position, king_from, enemy) ||
		    jc_is_attacked(position, crossed, enemy) ||
		    jc_is_attacked(position, king_to, enemy)) {
			continue;
		}
		emit(list, king_from, king_to, JC_EMPTY, JC_MOVE_CASTLE);
	}
}

// ---------------------------------------------------------------------------
// The whole position
// ---------------------------------------------------------------------------

static void generate(const JcPosition *position, JcMoveList *list, bool quiet) {
	const uint8_t colour = position->side;
	list->count = 0;
	const uint8_t count = position->roster_count[colour];
	for (uint8_t slot = 0; slot < count; slot++) {
		const uint8_t from = position->roster[colour][slot];
		switch (JC_KIND(position->board[from])) {
		case JC_PAWN:
			pawn_moves(position, list, from, colour, quiet);
			break;
		case JC_KNIGHT:
			leaper_moves(position, list, from, JC_KNIGHT_LEAPS, 8, colour, quiet);
			break;
		case JC_BISHOP:
			slider_moves(position, list, from, JC_DIAGONALS_FIRST, JC_DIAGONALS, colour, quiet);
			break;
		case JC_ROOK:
			slider_moves(position, list, from, JC_STRAIGHTS_FIRST, JC_STRAIGHTS, colour, quiet);
			break;
		case JC_QUEEN:
			slider_moves(position, list, from, JC_DIAGONALS_FIRST, JC_DIAGONALS + JC_STRAIGHTS, colour, quiet);
			break;
		case JC_KING:
			leaper_moves(position, list, from, JC_DIRECTIONS, 8, colour, quiet);
			break;
		default:
			break;
		}
	}
	if (quiet) {
		castle_moves(position, list, colour, position->castling);
	}
}

void jc_generate(const JcPosition *position, JcMoveList *list) {
	generate(position, list, true);
}

void jc_generate_captures(const JcPosition *position, JcMoveList *list) {
	generate(position, list, false);
}

void jc_generate_legal(const JcPosition *position, JcMoveList *list) {
	JcMoveList all;
	jc_generate(position, &all);
	JcPosition scratch = *position;
	list->count = 0;
	for (int i = 0; i < all.count; i++) {
		JcUndo undo;
		if (jc_make_if_legal(&scratch, all.moves[i], &undo)) {
			jc_unmake(&scratch, all.moves[i], &undo);
			list->moves[list->count] = all.moves[i];
			list->scores[list->count] = 0;
			list->count++;
		}
	}
}

uint64_t jc_perft(JcPosition *position, int depth) {
	if (depth <= 0) {
		return 1;
	}
	JcMoveList list;
	jc_generate(position, &list);
	uint64_t nodes = 0;
	for (int i = 0; i < list.count; i++) {
		JcUndo undo;
		if (!jc_make_if_legal(position, list.moves[i], &undo)) {
			continue;
		}
		// At the last ply the move itself is the leaf; there is nothing to
		// gain by generating the replies only to count them.
		nodes += depth == 1 ? 1 : jc_perft(position, depth - 1);
		jc_unmake(position, list.moves[i], &undo);
	}
	return nodes;
}
