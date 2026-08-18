// carts/jetchess/engine/chess.h
//
// The vocabulary the whole engine speaks: a colour, a piece, a square, a move
// and a position. Nothing here draws, makes a sound or reads the mouse — this
// half of JetChess is pure chess and would run anywhere.
//
// THE BOARD IS 0x88. The classic trick, and the reason this engine is small
// and quick. Sixty-four real squares are stored in an array of 128, laid out
// as eight rows of sixteen: eight squares that exist, then eight that do not.
//
//        a  b  c  d  e  f  g  h      (and then the phantom half)
//   8 | 70 71 72 73 74 75 76 77 | 78 79 7A 7B 7C 7D 7E 7F
//   7 | 60 61 62 63 64 65 66 67 | 68 ...
//   ...
//   1 | 00 01 02 03 04 05 06 07 | 08 ...
//
// A square number's low nibble is its file and its high nibble is its rank, so
// `square & 7` is the file, `square >> 4` is the rank, and — the whole point —
// a square is ON the board exactly when `square & 0x88` is zero. Step a knight
// off the edge and the result lands in the phantom half, where that one test
// catches it. No bounds arithmetic, no wrapping from h-file to a-file, one AND.
#ifndef JETCHESS_ENGINE_CHESS_H
#define JETCHESS_ENGINE_CHESS_H

#include "engine/portable.h"

// ---------------------------------------------------------------------------
// Colours
// ---------------------------------------------------------------------------

#define JC_WHITE 0
#define JC_BLACK 1
#define JC_COLOURS 2

// The other side. A colour is 0 or 1, so flipping it is one exclusive-or.
#define JC_OPPONENT(colour) ((colour) ^ 1)

// ---------------------------------------------------------------------------
// Pieces
// ---------------------------------------------------------------------------
//
// A piece is one byte: the low three bits are what it is, bit 3 is whose it
// is. So a white knight is 2 and a black knight is 10, and both answer 2 to
// JC_KIND. Empty is zero, which is no kind and no colour — and which is also
// why every "is there a piece here?" test is just the byte itself.

#define JC_EMPTY 0
#define JC_PAWN 1
#define JC_KNIGHT 2
#define JC_BISHOP 3
#define JC_ROOK 4
#define JC_QUEEN 5
#define JC_KING 6
#define JC_KINDS 7 // one past the last kind, so tables sized by kind fit

#define JC_COLOUR_BIT 8

#define JC_PIECE(colour, kind) ((uint8_t)((kind) | ((colour) << 3)))
#define JC_KIND(piece) ((piece) & 7)
#define JC_COLOUR_OF(piece) (((piece) >> 3) & 1)

// ---------------------------------------------------------------------------
// Squares
// ---------------------------------------------------------------------------

#define JC_BOARD_SLOTS 128 // the 0x88 array: 64 real squares and 64 phantom
#define JC_SQUARES 64      // squares that actually exist

// A square number that is not a square. 0x7F fails the 0x88 test, so it can
// never be mistaken for a real one, and it is what "no en-passant square" and
// "nothing is selected" both mean.
#define JC_NO_SQUARE 0x7F

#define JC_SQUARE(file, rank) ((uint8_t)(((rank) << 4) | (file)))
#define JC_FILE_OF(square) ((square) & 7)
#define JC_RANK_OF(square) ((square) >> 4)
#define JC_ON_BOARD(square) (((square) & 0x88) == 0)

// The rank a pawn of this colour promotes on, and the one it starts from.
#define JC_PROMOTION_RANK(colour) ((colour) == JC_WHITE ? 7 : 0)
#define JC_PAWN_RANK(colour) ((colour) == JC_WHITE ? 1 : 6)

// How far one step forward is, for each colour. A rank is sixteen slots.
#define JC_FORWARD(colour) ((colour) == JC_WHITE ? 16 : -16)

// ---------------------------------------------------------------------------
// Castling rights
// ---------------------------------------------------------------------------

#define JC_CASTLE_WHITE_KING (1 << 0)
#define JC_CASTLE_WHITE_QUEEN (1 << 1)
#define JC_CASTLE_BLACK_KING (1 << 2)
#define JC_CASTLE_BLACK_QUEEN (1 << 3)
#define JC_CASTLE_ALL 0x0F

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------
//
// Four bytes, which is small enough that a whole move list is a cache line or
// three. `promotion` is JC_EMPTY unless a pawn is being exchanged.

#define JC_MOVE_CAPTURE (1 << 0)
#define JC_MOVE_DOUBLE_PUSH (1 << 1) // the two-square opening push: sets en passant
#define JC_MOVE_EN_PASSANT (1 << 2)  // a capture whose victim is not on the target square
#define JC_MOVE_CASTLE (1 << 3)      // the king's two-square step; the rook moves with it
#define JC_MOVE_PROMOTION (1 << 4)

typedef struct {
	uint8_t from;
	uint8_t to;
	uint8_t promotion; // JC_EMPTY, or the kind the pawn becomes
	uint8_t flags;
} JcMove;

// The most moves any legal chess position has ever offered is 218, so a list
// this long can never overflow — and a fixed list means the engine allocates
// nothing, ever.
#define JC_MAX_MOVES 256

typedef struct {
	JcMove moves[JC_MAX_MOVES];
	int16_t scores[JC_MAX_MOVES]; // move-ordering scores, filled by the search
	int count;
} JcMoveList;

// Two moves are the same move when they start and end alike and exchange the
// same piece. Flags are derived from the position, so they never differ when
// these three agree.
static inline bool jc_move_same(JcMove a, JcMove b) {
	return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

// The move that is no move: used for "the search found nothing" and for the
// empty slot in a transposition entry.
static inline JcMove jc_move_none(void) {
	JcMove move = {JC_NO_SQUARE, JC_NO_SQUARE, JC_EMPTY, 0};
	return move;
}

static inline bool jc_move_is_none(JcMove move) {
	return move.from == JC_NO_SQUARE;
}

// ---------------------------------------------------------------------------
// The position
// ---------------------------------------------------------------------------
//
// Everything about one moment of a game. Beside the board itself it carries a
// *roster*: the squares each side's pieces stand on, so generating moves and
// counting material walk sixteen entries rather than sixty-four squares. The
// roster is maintained in exactly three places (board.c's add, lift and shift)
// and is a pure mirror of the board — which `jc_position_is_sound` checks.

#define JC_ROSTER_SLOTS 16 // a side can never have more than sixteen pieces

typedef struct {
	uint8_t board[JC_BOARD_SLOTS]; // the pieces, by 0x88 square
	uint8_t roster[JC_COLOURS][JC_ROSTER_SLOTS]; // each side's occupied squares
	uint8_t roster_count[JC_COLOURS];
	uint8_t roster_slot[JC_BOARD_SLOTS]; // square -> where it sits in its roster
	uint8_t king[JC_COLOURS];            // each king's square, kept to hand
	uint8_t side;                        // who moves
	uint8_t castling;                    // the four rights, as bits
	uint8_t en_passant;                  // the square a pawn may be taken ON, or none
	uint16_t halfmove;                   // plies since a pawn moved or a piece fell
	uint16_t fullmove;                   // the move number a scoresheet would show
	uint64_t key;                        // Zobrist hash of everything above
} JcPosition;

// What `jc_make` must put back to undo itself. The search keeps a stack of
// these, one per ply, so unmaking is a copy rather than a recomputation.
typedef struct {
	uint8_t captured;   // the piece that came off, JC_EMPTY if none
	uint8_t castling;   // the rights before the move
	uint8_t en_passant; // the en-passant square before the move
	uint16_t halfmove;  // the fifty-move clock before the move
	uint64_t key;       // the hash before the move
} JcUndo;

// ---------------------------------------------------------------------------
// board.c — setting a position up, and moving through it
// ---------------------------------------------------------------------------

// Build every table the engine reads: the Zobrist keys and the attack tables.
// Call once, before anything else. It is pure: the same tables every run, on
// every machine, which is what lets the opening book be keyed by hash.
void jc_engine_init(void);

// The starting array, ready to play.
void jc_position_start(JcPosition *position);

// Set a position up from Forsyth-Edwards notation. Returns false — and leaves
// the position untouched — if the text does not describe a legal array.
bool jc_position_from_fen(JcPosition *position, const char *fen);

// Write the position as FEN into `out`, which must hold at least
// JC_FEN_LENGTH bytes. Returns `out`.
#define JC_FEN_LENGTH 96
char *jc_position_to_fen(const JcPosition *position, char *out);

// Play a move. Fills `undo` with what it takes to put the position back.
// The move must be one jc_generate produced for this position.
void jc_make(JcPosition *position, JcMove move, JcUndo *undo);

// Put the position back exactly as it was before `move`.
void jc_unmake(JcPosition *position, JcMove move, const JcUndo *undo);

// Play a move and keep it only if it leaves the mover's own king safe.
// Returns false, with the position untouched, when it does not.
bool jc_make_if_legal(JcPosition *position, JcMove move, JcUndo *undo);

// Pass the turn without moving a piece — the null move the search uses to ask
// "even if I do nothing, is this position already winning?".
void jc_make_null(JcPosition *position, JcUndo *undo);
void jc_unmake_null(JcPosition *position, const JcUndo *undo);

// The roster and the board agree, both kings are present, and the hash matches
// a fresh computation. Used by the tests; cheap enough to assert in anger.
bool jc_position_is_sound(const JcPosition *position);

// ---------------------------------------------------------------------------
// attack.c — who is hitting what
// ---------------------------------------------------------------------------

// Is `square` attacked by any piece of `by`? The workhorse: legality, check
// detection and king safety all reduce to this one question.
bool jc_is_attacked(const JcPosition *position, uint8_t square, uint8_t by);

// Is the side to move in check?
static inline bool jc_in_check(const JcPosition *position) {
	return jc_is_attacked(position, position->king[position->side], JC_OPPONENT(position->side));
}

// ---------------------------------------------------------------------------
// movegen.c — the moves a position offers
// ---------------------------------------------------------------------------

// Every pseudo-legal move for the side to move: legal but for the possibility
// that it leaves its own king in check, which `jc_make_if_legal` settles.
void jc_generate(const JcPosition *position, JcMoveList *list);

// Captures and promotions only — what the quiescence search looks at.
void jc_generate_captures(const JcPosition *position, JcMoveList *list);

// Every move that is legal outright. Slower than jc_generate (it plays each
// move to check), and meant for the rules, the interface and the tests rather
// than the search's inner loop.
void jc_generate_legal(const JcPosition *position, JcMoveList *list);

// How many legal move sequences of exactly `depth` plies this position has —
// the standard correctness measure for a move generator. Perft numbers for
// well-known positions are published to twelve digits, so a generator that
// matches them is right, and one that does not is wrong by a knowable amount.
uint64_t jc_perft(JcPosition *position, int depth);

// ---------------------------------------------------------------------------
// notation.c — chess as people write it
// ---------------------------------------------------------------------------

// "e4", "h8" — two characters and a terminator.
#define JC_SQUARE_NAME_LENGTH 3
char *jc_square_name(uint8_t square, char *out);

// Write an unsigned number as decimal text at `out`, with no terminator, and
// answer how many characters it took. The cart has no stdio, and every number
// it shows a player goes through here.
int jc_uint_to_text(unsigned value, char *out);

// A move in standard algebraic notation: "Nf3", "exd5", "O-O", "e8=Q+",
// "Qxf7#". `position` must be the one the move is played in, because SAN is
// only meaningful in context: whether another piece could have gone there
// decides whether the move needs its file, its rank or neither.
#define JC_SAN_LENGTH 10
char *jc_move_to_san(const JcPosition *position, JcMove move, char *out);

// The move this SAN names, or a none-move if the text names no legal move.
// It works by *writing*: every legal move is rendered and compared, so the
// reader can never disagree with the writer about what a move is called.
JcMove jc_move_from_san(const JcPosition *position, const char *san);

// ---------------------------------------------------------------------------
// eval.c — what a position is worth
// ---------------------------------------------------------------------------

// Centipawns, from the point of view of the side to move. A pawn is about 100.
int jc_evaluate(const JcPosition *position);

// What one piece is worth, for move ordering and for the material readout.
int jc_piece_value(uint8_t kind);

// Neither side can force mate: king against king, king and a minor against
// king, or king and bishop against king and bishop on one colour.
bool jc_is_insufficient_material(const JcPosition *position);

#endif // JETCHESS_ENGINE_CHESS_H
