// carts/jetchess/engine/eval.c
//
// What a position is worth, in centipawns, from the point of view of whoever
// is to move. A pawn is 100, so +150 means "a pawn and a half up".
//
// The evaluation has three parts, and they are in order of how much they
// matter:
//
//   1. MATERIAL. Who has more wood. This decides nearly every game.
//   2. PLACEMENT. The same piece is worth more on a good square than a bad
//      one: a knight in the centre, a rook on the seventh, a king tucked
//      behind its pawns. One table per kind says how much more.
//   3. PAWNS AND FILES. Doubled and isolated pawns are weak, passed pawns are
//      dangerous, and a rook likes a file with no pawn on it.
//
// TAPERING. A king belongs behind three pawns in the middlegame and in the
// middle of the board in the endgame, and there is no single number for that.
// So every table is written twice, once for each, and the score is mixed
// between them by how much material is still on: a "phase" that starts at 24
// and falls toward 0 as pieces come off. The endgame arrives gradually, which
// is how it actually feels to play.
#include "chess.h"

// ---------------------------------------------------------------------------
// Material
// ---------------------------------------------------------------------------

// The classical values, with the bishop a shade over the knight. The king has
// no value: he is never exchanged, and giving him one would only risk an
// overflow in a position where one side is somehow missing him.
static const int16_t JC_MATERIAL[JC_KINDS] = {0, 100, 320, 330, 500, 900, 0};

int jc_piece_value(uint8_t kind) {
	return JC_MATERIAL[kind & 7];
}

// How much of the middlegame each piece keeps alive. Pawns count for nothing:
// an endgame is defined by the absence of PIECES, not of pawns.
static const int JC_PHASE_WEIGHT[JC_KINDS] = {0, 0, 1, 1, 2, 4, 0};

// A full board, counted over both sides: four knights at one each, four
// bishops at one, four rooks at two and two queens at four — 4 + 4 + 8 + 8.
#define JC_PHASE_MAX 24

// ---------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------
//
// Each table is written the way a board is drawn: the eighth rank on the first
// line, the first rank on the last, White playing up the page. White reads it
// mirrored top to bottom and Black reads it as written, so one table serves
// both sides.

#define JC_TABLE(name) static const int16_t name[JC_SQUARES]

// A pawn's worth is almost all in how far it has come, and in the middlegame
// also in whether it is holding the centre. The dip at d2/e2 is deliberate: a
// pawn still sitting there is blocking a bishop.
JC_TABLE(PAWN_MG) = {
	0,    0,    0,    0,    0,    0,    0,    0,
	60,   60,   60,   60,   60,   60,   60,   60,
	12,   18,   26,   34,   34,   26,   18,   12,
	4,    8,   14,   28,   28,   14,    8,    4,
	0,    2,    6,   20,   20,    4,    2,    0,
	4,   -2,   -6,    2,    2,   -8,   -2,    4,
	4,    8,    8,  -18,  -18,    8,    8,    4,
	0,    0,    0,    0,    0,    0,    0,    0,
};
JC_TABLE(PAWN_EG) = {
	0,    0,    0,    0,    0,    0,    0,    0,
	110,  110,  110,  110,  110,  110,  110,  110,
	58,   60,   56,   52,   52,   56,   60,   58,
	28,   30,   26,   22,   22,   26,   30,   28,
	12,   14,   12,   10,   10,   12,   14,   12,
	4,    6,    6,    6,    6,    6,    6,    4,
	0,    0,    0,    0,    0,    0,    0,    0,
	0,    0,    0,    0,    0,    0,    0,    0,
};

// A knight on the rim is dim: the edges are punished hard, because a knight
// there reaches half as many squares.
JC_TABLE(KNIGHT_MG) = {
	-50,  -40,  -30,  -30,  -30,  -30,  -40,  -50,
	-40,  -20,    0,    6,    6,    0,  -20,  -40,
	-30,    6,   14,   18,   18,   14,    6,  -30,
	-30,    2,   18,   22,   22,   18,    2,  -30,
	-30,    0,   18,   22,   22,   18,    0,  -30,
	-30,    4,   14,   18,   18,   14,    4,  -30,
	-40,  -20,    0,    4,    4,    0,  -20,  -40,
	-50,  -40,  -30,  -30,  -30,  -30,  -40,  -50,
};

// A bishop wants the long diagonals and dislikes its own back rank.
JC_TABLE(BISHOP_MG) = {
	-20,  -10,  -10,  -10,  -10,  -10,  -10,  -20,
	-10,    0,    0,    0,    0,    0,    0,  -10,
	-10,    0,    6,   12,   12,    6,    0,  -10,
	-10,    6,    6,   12,   12,    6,    6,  -10,
	-10,    0,   12,   12,   12,   12,    0,  -10,
	-10,   12,   12,   12,   12,   12,   12,  -10,
	-10,    6,    0,    0,    0,    0,    6,  -10,
	-20,  -10,  -10,  -10,  -10,  -10,  -10,  -20,
};

// A rook wants the seventh rank, and wants to be near the middle files where
// the open lines usually are. The last row nudges it toward castling.
JC_TABLE(ROOK_MG) = {
	0,    0,    0,    0,    0,    0,    0,    0,
	10,   20,   20,   20,   20,   20,   20,   10,
	-4,    0,    0,    0,    0,    0,    0,   -4,
	-4,    0,    0,    0,    0,    0,    0,   -4,
	-4,    0,    0,    0,    0,    0,    0,   -4,
	-4,    0,    0,    0,    0,    0,    0,   -4,
	-4,    0,    0,    0,    0,    0,    0,   -4,
	0,    0,    4,   10,   10,    4,    0,    0,
};

// The queen is worth so much that where she stands barely matters — except
// that bringing her out early loses time, which the back-rank corners say.
JC_TABLE(QUEEN_MG) = {
	-20,  -10,  -10,   -4,   -4,  -10,  -10,  -20,
	-10,    0,    0,    0,    0,    0,    0,  -10,
	-10,    0,    4,    4,    4,    4,    0,  -10,
	-4,    0,    4,    4,    4,    4,    0,   -4,
	0,    0,    4,    4,    4,    4,    0,   -4,
	-10,    4,    4,    4,    4,    4,    0,  -10,
	-10,    0,    4,    0,    0,    0,    0,  -10,
	-20,  -10,  -10,   -4,   -4,  -10,  -10,  -20,
};

// The two faces of a king. In the middlegame he hides in a corner behind his
// pawns; in the endgame he is a strong piece and belongs in the centre. This
// one pair of tables is the whole reason the evaluation is tapered.
JC_TABLE(KING_MG) = {
	-60,  -70,  -70,  -80,  -80,  -70,  -70,  -60,
	-60,  -70,  -70,  -80,  -80,  -70,  -70,  -60,
	-60,  -70,  -70,  -80,  -80,  -70,  -70,  -60,
	-60,  -70,  -70,  -80,  -80,  -70,  -70,  -60,
	-40,  -50,  -50,  -60,  -60,  -50,  -50,  -40,
	-20,  -30,  -30,  -40,  -40,  -30,  -30,  -20,
	20,   20,   -8,   -8,   -8,   -8,   20,   20,
	20,   34,   12,    0,    0,   10,   36,   20,
};
JC_TABLE(KING_EG) = {
	-50,  -40,  -30,  -20,  -20,  -30,  -40,  -50,
	-30,  -20,  -10,    0,    0,  -10,  -20,  -30,
	-30,  -10,   20,   30,   30,   20,  -10,  -30,
	-30,  -10,   30,   40,   40,   30,  -10,  -30,
	-30,  -10,   30,   40,   40,   30,  -10,  -30,
	-30,  -10,   20,   30,   30,   20,  -10,  -30,
	-30,  -30,    0,    0,    0,    0,  -30,  -30,
	-50,  -30,  -30,  -30,  -30,  -30,  -30,  -50,
};

// Which table each kind reads, for each half of the game. The knight, bishop,
// rook and queen use one table for both: their best squares hardly change.
static const int16_t *const JC_PLACEMENT[2][JC_KINDS] = {
	{NULL, PAWN_MG, KNIGHT_MG, BISHOP_MG, ROOK_MG, QUEEN_MG, KING_MG},
	{NULL, PAWN_EG, KNIGHT_MG, BISHOP_MG, ROOK_MG, QUEEN_MG, KING_EG},
};
#define JC_MIDDLEGAME 0
#define JC_ENDGAME 1

// A 0x88 square as an index into a table written the way a board is drawn.
// White reads it upside down, which is what makes one table serve both sides.
static int table_index(uint8_t square, uint8_t colour) {
	const int file = JC_FILE_OF(square);
	const int rank = JC_RANK_OF(square);
	return colour == JC_WHITE ? (7 - rank) * 8 + file : rank * 8 + file;
}

// ---------------------------------------------------------------------------
// Pawns and files
// ---------------------------------------------------------------------------

#define JC_BISHOP_PAIR_MG 30
#define JC_BISHOP_PAIR_EG 46
#define JC_DOUBLED_PAWN -14
#define JC_ISOLATED_PAWN -16
#define JC_ROOK_OPEN_FILE 20
#define JC_ROOK_SEMI_OPEN_FILE 10
#define JC_TEMPO 10

// What a passed pawn is worth, by how many ranks it has advanced from its own
// second rank. It climbs steeply, because a pawn on the seventh is nearly a
// queen. Index 0 and 7 never occur (a pawn is never on its own first rank and
// is a queen by the eighth), and are there so the table is a board's height.
static const int16_t JC_PASSED_PAWN[8] = {0, 8, 14, 28, 52, 88, 140, 0};

// One side's pawns, gathered once so the structure terms are a table lookup
// rather than a second walk of the board.
typedef struct {
	uint8_t count[8];    // pawns on each file
	uint8_t hindmost[8]; // the LEAST advanced pawn on each file, as an advance
} JcPawnFiles;

static void gather_pawns(const JcPosition *position, uint8_t colour, JcPawnFiles *files) {
	for (int file = 0; file < 8; file++) {
		files->count[file] = 0;
		files->hindmost[file] = 7;
	}
	const uint8_t total = position->roster_count[colour];
	for (uint8_t slot = 0; slot < total; slot++) {
		const uint8_t square = position->roster[colour][slot];
		if (JC_KIND(position->board[square]) != JC_PAWN) {
			continue;
		}
		const int file = JC_FILE_OF(square);
		// "Advance" measured from each side's own first rank, so the two
		// colours can be compared with the same numbers.
		const int advance = colour == JC_WHITE ? JC_RANK_OF(square) : 7 - JC_RANK_OF(square);
		files->count[file]++;
		// The hindmost enemy pawn on a file is the one most likely to still
		// be in front of ours, so it is the only one the passed test needs.
		if (advance < files->hindmost[file]) {
			files->hindmost[file] = (uint8_t)advance;
		}
	}
}

// Nothing of the enemy's can stop this pawn: no enemy pawn on its own file or
// on either neighbour is still in front of it.
//
// Both sides count their advance from their own first rank, so an enemy pawn
// at `their advance` stands on the rank seven minus that. It is in front of
// ours exactly when `7 - theirs > ours`, which is the one line below.
static bool is_passed(const JcPawnFiles *enemy, int file, int advance) {
	for (int near = file - 1; near <= file + 1; near++) {
		if (near < 0 || near > 7 || enemy->count[near] == 0) {
			continue;
		}
		if (enemy->hindmost[near] + advance < 7) {
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// The evaluation itself
// ---------------------------------------------------------------------------

// One side's contribution, added into the running middlegame and endgame
// totals. Kept as one function so the two colours cannot drift apart.
static void score_side(const JcPosition *position, uint8_t colour, const JcPawnFiles *own,
                       const JcPawnFiles *enemy, int *middlegame, int *endgame, int *phase) {
	int mg = 0, eg = 0;
	int bishops = 0;
	const uint8_t total = position->roster_count[colour];
	for (uint8_t slot = 0; slot < total; slot++) {
		const uint8_t square = position->roster[colour][slot];
		const uint8_t kind = JC_KIND(position->board[square]);
		const int at = table_index(square, colour);
		const int value = JC_MATERIAL[kind];
		mg += value + JC_PLACEMENT[JC_MIDDLEGAME][kind][at];
		eg += value + JC_PLACEMENT[JC_ENDGAME][kind][at];
		*phase += JC_PHASE_WEIGHT[kind];

		const int file = JC_FILE_OF(square);
		switch (kind) {
		case JC_BISHOP:
			bishops++;
			break;
		case JC_ROOK:
			// An open file has no pawn of anybody's; a half-open one has none
			// of ours, which is nearly as good.
			if (own->count[file] == 0) {
				const int bonus = enemy->count[file] == 0 ? JC_ROOK_OPEN_FILE : JC_ROOK_SEMI_OPEN_FILE;
				mg += bonus;
				eg += bonus / 2;
			}
			break;
		case JC_PAWN: {
				const int advance = colour == JC_WHITE ? JC_RANK_OF(square) : 7 - JC_RANK_OF(square);
				if (own->count[file] > 1) {
					// Charged once per pawn on the file, which is what makes a
					// tripled file three times as sore as a doubled one.
					mg += JC_DOUBLED_PAWN;
					eg += JC_DOUBLED_PAWN;
				}
				const bool has_neighbour = (file > 0 && own->count[file - 1]) ||
				                           (file < 7 && own->count[file + 1]);
				if (!has_neighbour) {
					mg += JC_ISOLATED_PAWN;
					eg += JC_ISOLATED_PAWN;
				}
				if (is_passed(enemy, file, advance)) {
					mg += JC_PASSED_PAWN[advance] / 2;
					eg += JC_PASSED_PAWN[advance];
				}
				break;
			}
		default:
			break;
		}
	}
	if (bishops >= 2) {
		mg += JC_BISHOP_PAIR_MG;
		eg += JC_BISHOP_PAIR_EG;
	}
	*middlegame += mg;
	*endgame += eg;
}

int jc_evaluate(const JcPosition *position) {
	JcPawnFiles pawns[JC_COLOURS];
	gather_pawns(position, JC_WHITE, &pawns[JC_WHITE]);
	gather_pawns(position, JC_BLACK, &pawns[JC_BLACK]);

	int middlegame = 0, endgame = 0, phase = 0;
	score_side(position, JC_WHITE, &pawns[JC_WHITE], &pawns[JC_BLACK], &middlegame, &endgame, &phase);
	int black_mg = 0, black_eg = 0;
	score_side(position, JC_BLACK, &pawns[JC_BLACK], &pawns[JC_WHITE], &black_mg, &black_eg, &phase);
	middlegame -= black_mg;
	endgame -= black_eg;

	if (phase > JC_PHASE_MAX) {
		phase = JC_PHASE_MAX; // promotions can put more on the board than the start
	}
	// The mix: all middlegame at a full board, all endgame at a bare one.
	int score = (middlegame * phase + endgame * (JC_PHASE_MAX - phase)) / JC_PHASE_MAX;

	// Everything above was written from White's side. Turn it round if Black
	// is to move, then pay the side to move a small tempo bonus — having the
	// move is worth something, and saying so keeps the search from thinking a
	// quiet position is exactly level and shuffling.
	if (position->side == JC_BLACK) {
		score = -score;
	}
	return score + JC_TEMPO;
}

// ---------------------------------------------------------------------------
// Draws nobody can win
// ---------------------------------------------------------------------------

bool jc_is_insufficient_material(const JcPosition *position) {
	int minors[JC_COLOURS] = {0, 0};
	int bishop_square_colour[JC_COLOURS] = {-1, -1};
	for (uint8_t colour = 0; colour < JC_COLOURS; colour++) {
		const uint8_t total = position->roster_count[colour];
		for (uint8_t slot = 0; slot < total; slot++) {
			const uint8_t square = position->roster[colour][slot];
			switch (JC_KIND(position->board[square])) {
			case JC_KING:
				break;
			case JC_BISHOP:
				// Which colour square it stands on: file and rank of the same
				// parity is a dark square.
				bishop_square_colour[colour] = (JC_FILE_OF(square) + JC_RANK_OF(square)) & 1;
				minors[colour]++;
				break;
			case JC_KNIGHT:
				minors[colour]++;
				break;
			default:
				return false; // a pawn, a rook or a queen can always mate
			}
		}
	}
	if (minors[JC_WHITE] <= 1 && minors[JC_BLACK] == 0) {
		return true; // king, or king and one minor, against a bare king
	}
	if (minors[JC_BLACK] <= 1 && minors[JC_WHITE] == 0) {
		return true;
	}
	// The one remaining draw: a bishop each, both on squares of one colour.
	return minors[JC_WHITE] == 1 && minors[JC_BLACK] == 1 &&
	       bishop_square_colour[JC_WHITE] >= 0 &&
	       bishop_square_colour[JC_WHITE] == bishop_square_colour[JC_BLACK];
}
