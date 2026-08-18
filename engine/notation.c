// carts/jetchess/engine/notation.c
//
// Chess as people write it: "e4", "Nf3", "exd5", "O-O", "Qxf7#".
//
// Standard algebraic notation names a move by its destination and by the least
// that distinguishes it. That "least" is the interesting part: "Nf3" is enough
// only when one knight can reach f3, and when two can it becomes "Nbd2" or
// "N1d2" or, in the rare case of three, "Nb1d2". So a move cannot be written
// without looking at the position it is played in — which is why every call
// here takes one.
//
// READING IS DONE BY WRITING. `jc_move_from_san` does not parse. It generates
// every legal move, writes each one out, and returns the one whose text
// matches. That is slower than a parser and it is the right trade: a parser is
// a second opinion about what a move is called, and a second opinion is a bug
// waiting for the one position where the two disagree. It is only ever used to
// read the opening book at startup, where a few thousand comparisons cost
// under a millisecond.
#include "chess.h"

// The letter for each kind. A pawn's letter is never written, so its slot is a
// space that the writer skips.
static const char JC_SAN_LETTER[JC_KINDS] = {'?', ' ', 'N', 'B', 'R', 'Q', 'K'};

int jc_uint_to_text(unsigned value, char *out) {
	char digits[10];
	int length = 0;
	do {
		digits[length++] = (char)('0' + value % 10);
		value /= 10;
	} while (value);
	for (int i = 0; i < length; i++) {
		out[i] = digits[length - 1 - i];
	}
	return length;
}

char *jc_square_name(uint8_t square, char *out) {
	out[0] = (char)('a' + JC_FILE_OF(square));
	out[1] = (char)('1' + JC_RANK_OF(square));
	out[2] = '\0';
	return out;
}

// Does the move leave the opponent in check, and is it mate? Answered by
// playing it, because there is no cheaper honest way to know.
static void check_and_mate(const JcPosition *position, JcMove move, bool *check, bool *mate) {
	JcPosition after = *position;
	JcUndo undo;
	jc_make(&after, move, &undo);
	*check = jc_in_check(&after);
	*mate = false;
	if (!*check) {
		return;
	}
	JcMoveList replies;
	jc_generate_legal(&after, &replies);
	*mate = replies.count == 0;
}

char *jc_move_to_san(const JcPosition *position, JcMove move, char *out) {
	char *pen = out;
	const uint8_t piece = position->board[move.from];
	const uint8_t kind = JC_KIND(piece);

	if (move.flags & JC_MOVE_CASTLE) {
		// The king's destination file says which way it went.
		const char *text = JC_FILE_OF(move.to) == 6 ? "O-O" : "O-O-O";
		while (*text) {
			*pen++ = *text++;
		}
	} else if (kind == JC_PAWN) {
		// A pawn names its file only when it captures.
		if (move.flags & JC_MOVE_CAPTURE) {
			*pen++ = (char)('a' + JC_FILE_OF(move.from));
			*pen++ = 'x';
		}
		pen = jc_square_name(move.to, pen) + 2;
		if (move.flags & JC_MOVE_PROMOTION) {
			*pen++ = '=';
			*pen++ = JC_SAN_LETTER[move.promotion];
		}
	} else {
		*pen++ = JC_SAN_LETTER[kind];
		// Disambiguation: look for another piece of the same kind that could
		// also have landed here legally.
		JcMoveList legal;
		jc_generate_legal(position, &legal);
		bool ambiguous = false, same_file = false, same_rank = false;
		for (int i = 0; i < legal.count; i++) {
			const JcMove other = legal.moves[i];
			if (other.to != move.to || other.from == move.from) {
				continue;
			}
			if (JC_KIND(position->board[other.from]) != kind) {
				continue;
			}
			ambiguous = true;
			same_file |= JC_FILE_OF(other.from) == JC_FILE_OF(move.from);
			same_rank |= JC_RANK_OF(other.from) == JC_RANK_OF(move.from);
		}
		if (ambiguous) {
			// The file alone, unless a rival shares it; then the rank; and if
			// a rival shares that too, both.
			if (!same_file) {
				*pen++ = (char)('a' + JC_FILE_OF(move.from));
			} else if (!same_rank) {
				*pen++ = (char)('1' + JC_RANK_OF(move.from));
			} else {
				*pen++ = (char)('a' + JC_FILE_OF(move.from));
				*pen++ = (char)('1' + JC_RANK_OF(move.from));
			}
		}
		if (move.flags & JC_MOVE_CAPTURE) {
			*pen++ = 'x';
		}
		pen = jc_square_name(move.to, pen) + 2;
	}

	bool check = false, mate = false;
	check_and_mate(position, move, &check, &mate);
	if (mate) {
		*pen++ = '#';
	} else if (check) {
		*pen++ = '+';
	}
	*pen = '\0';
	return out;
}

// Two pieces of notation name the same move when they agree on everything but
// the decoration — the check mark, and the exclamation marks an annotator
// adds. Comparing with those stripped lets the opening book be written the way
// a book is written.
static bool san_matches(const char *a, const char *b) {
	for (;;) {
		while (*a == '+' || *a == '#' || *a == '!' || *a == '?') {
			a++;
		}
		while (*b == '+' || *b == '#' || *b == '!' || *b == '?') {
			b++;
		}
		if (*a != *b) {
			return false;
		}
		if (*a == '\0') {
			return true;
		}
		a++;
		b++;
	}
}

JcMove jc_move_from_san(const JcPosition *position, const char *san) {
	JcMoveList legal;
	jc_generate_legal(position, &legal);
	for (int i = 0; i < legal.count; i++) {
		char written[JC_SAN_LENGTH];
		jc_move_to_san(position, legal.moves[i], written);
		if (san_matches(written, san)) {
			return legal.moves[i];
		}
	}
	return jc_move_none();
}
