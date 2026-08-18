// carts/jetchess/engine/attack.c
//
// "Is this square attacked?" — the one question the rules of chess keep asking.
// Legality is it (a move is illegal if it leaves your own king attacked), check
// is it, checkmate is it, and castling asks it three times.
//
// The naive answer walks every enemy piece and generates its moves. The 0x88
// answer is a lookup. Because a square's number encodes its file and rank, the
// DIFFERENCE between two squares says everything about how they are related:
// +17 is one step up-right, +33 is a knight's leap, +34 is two steps up-right.
// So two tables, indexed by that difference, answer both halves of the
// question at once:
//
//   COULD_ATTACK[difference]  which kinds of piece move along this line at all
//   STEP_ALONG[difference]    the single step that walks it, for the sliders
//
// A knight or a king needs only the first table: if the bit is set, it attacks.
// A bishop, rook or queen needs the second as well, to check nothing stands in
// the way. A pawn gets two bits of its own, because it is the one piece whose
// attacks depend on which way it is facing.
#include "chess.h"

#include "engine/internal.h"

// The difference between two on-board squares runs from -119 to +119, so a
// table of 240 entries indexed by `difference + 119` covers every pair.
#define JC_DIFFERENCE_BIAS 119
#define JC_DIFFERENCE_SLOTS 240

// One bit per kind of attacker. The queen has no bit of her own: she attacks
// exactly where a bishop and a rook do, so she simply carries both.
#define JC_ATK_PAWN_WHITE 0x01
#define JC_ATK_PAWN_BLACK 0x02
#define JC_ATK_KNIGHT 0x04
#define JC_ATK_BISHOP 0x08
#define JC_ATK_ROOK 0x10
#define JC_ATK_KING 0x20

// The eight directions, in the one order the whole engine uses: the four
// diagonals first, then the four straight lines. A bishop takes the first
// four, a rook the last four, a queen and a king all eight. Declared in
// engine/internal.h, and defined here because this is where they are built
// into the tables above.
const int8_t JC_DIRECTIONS[8] = {17, 15, -15, -17, 16, 1, -1, -16};

// A knight's eight leaps.
const int8_t JC_KNIGHT_LEAPS[8] = {33, 31, 18, 14, -14, -18, -31, -33};

static uint8_t s_could_attack[JC_DIFFERENCE_SLOTS];
static int8_t s_step_along[JC_DIFFERENCE_SLOTS];

// What each piece byte can do, so the test is one AND rather than a switch.
static uint8_t s_piece_attacks[16];

// Note one line of the table: every square `step` reaches from the origin, out
// to `range` steps, can be attacked along it by a piece carrying `kinds`.
static void note(int step, int range, uint8_t kinds) {
	for (int distance = 1; distance <= range; distance++) {
		const int slot = JC_DIFFERENCE_BIAS + step * distance;
		s_could_attack[slot] |= kinds;
		s_step_along[slot] = (int8_t)step;
	}
}

void jc_attack_tables_init(void) {
	memset(s_could_attack, 0, sizeof s_could_attack);
	memset(s_step_along, 0, sizeof s_step_along);

	for (int i = 0; i < JC_DIAGONALS; i++) {
		note(JC_DIRECTIONS[JC_DIAGONALS_FIRST + i], JC_SLIDE_RANGE, JC_ATK_BISHOP);
	}
	for (int i = 0; i < JC_STRAIGHTS; i++) {
		note(JC_DIRECTIONS[JC_STRAIGHTS_FIRST + i], JC_SLIDE_RANGE, JC_ATK_ROOK);
	}
	for (int i = 0; i < 8; i++) {
		note(JC_KNIGHT_LEAPS[i], 1, JC_ATK_KNIGHT);
		note(JC_DIRECTIONS[i], 1, JC_ATK_KING);
	}
	// A pawn attacks the two squares diagonally ahead of it, and only those.
	note(17, 1, JC_ATK_PAWN_WHITE);
	note(15, 1, JC_ATK_PAWN_WHITE);
	note(-17, 1, JC_ATK_PAWN_BLACK);
	note(-15, 1, JC_ATK_PAWN_BLACK);

	memset(s_piece_attacks, 0, sizeof s_piece_attacks);
	s_piece_attacks[JC_PIECE(JC_WHITE, JC_PAWN)] = JC_ATK_PAWN_WHITE;
	s_piece_attacks[JC_PIECE(JC_BLACK, JC_PAWN)] = JC_ATK_PAWN_BLACK;
	for (uint8_t colour = 0; colour < JC_COLOURS; colour++) {
		s_piece_attacks[JC_PIECE(colour, JC_KNIGHT)] = JC_ATK_KNIGHT;
		s_piece_attacks[JC_PIECE(colour, JC_BISHOP)] = JC_ATK_BISHOP;
		s_piece_attacks[JC_PIECE(colour, JC_ROOK)] = JC_ATK_ROOK;
		s_piece_attacks[JC_PIECE(colour, JC_QUEEN)] = JC_ATK_BISHOP | JC_ATK_ROOK;
		s_piece_attacks[JC_PIECE(colour, JC_KING)] = JC_ATK_KING;
	}
}

// Only these three kinds need the path checked; everything else leaps.
#define JC_SLIDERS (JC_ATK_BISHOP | JC_ATK_ROOK)

bool jc_is_attacked(const JcPosition *position, uint8_t square, uint8_t by) {
	const uint8_t count = position->roster_count[by];
	for (uint8_t slot = 0; slot < count; slot++) {
		const uint8_t from = position->roster[by][slot];
		const uint8_t piece = position->board[from];
		const int difference = JC_DIFFERENCE_BIAS + (int)square - (int)from;
		const uint8_t shared = s_could_attack[difference] & s_piece_attacks[piece];
		if (shared == 0) {
			continue;
		}
		if ((shared & JC_SLIDERS) == 0) {
			return true; // a pawn, a knight or a king: no path to clear
		}
		// A slider: walk the line and see whether anything stands in it.
		const int step = s_step_along[difference];
		int walk = (int)from + step;
		while (walk != (int)square && position->board[walk] == JC_EMPTY) {
			walk += step;
		}
		if (walk == (int)square) {
			return true;
		}
	}
	return false;
}
