// carts/jetchess/engine/board.c
//
// The position itself: how it is set up, how a move changes it, and how it is
// put back. Three ideas do all the work here.
//
// THE ROSTER. Beside the board, each side keeps a list of the squares its
// pieces stand on. Generating moves then walks sixteen entries instead of
// sixty-four squares, and the list is kept honest by routing every change
// through exactly three helpers: `place`, `lift` and `shift`. Nothing else in
// the cart writes to `board[]`.
//
// THE HASH. Every position carries a 64-bit Zobrist key: a random number per
// (piece, square), exclusive-ORed together, plus numbers for the side to move,
// the castling rights and the en-passant file. Because exclusive-or undoes
// itself, a move updates the key by cancelling what left and folding in what
// arrived — a handful of operations rather than a walk of the board. The key
// is what the transposition table, the repetition check and the opening book
// are all keyed by.
//
// EN PASSANT IS RECORDED ONLY WHEN IT CAN BE TAKEN. A double push sets the
// en-passant square only if an enemy pawn actually stands beside the pushed
// pawn. That is the FIDE reading of "the same position" — two positions differ
// by en passant only when the capture is really available — and it keeps the
// hash honest for repetition without a special case anywhere else.
#include "chess.h"

#include "engine/internal.h"

// ---------------------------------------------------------------------------
// The random numbers the hash is built from
// ---------------------------------------------------------------------------

// A fixed seed, so every run of every build hashes a position identically.
// The opening book is keyed by these numbers; a seed from the clock would mean
// a book that only worked until the next boot.
#define JC_ZOBRIST_SEED 0x9E3779B97F4A7C15ULL

// One key per piece byte per square. Piece bytes run 0..14 (the colour bit is
// bit 3), so fifteen rows cover every piece and waste one on "empty".
#define JC_PIECE_KEYS 16

static uint64_t s_piece_key[JC_PIECE_KEYS][JC_BOARD_SLOTS];
static uint64_t s_side_key;
static uint64_t s_castle_key[JC_CASTLE_ALL + 1];
static uint64_t s_en_passant_key[8];
static bool s_tables_ready;

// Built below, beside the rights they encode.
static void build_castle_masks(void);

// SplitMix64: a very small generator whose output is good enough for hashing
// and whose state is a single integer. Deterministic by construction.
static uint64_t splitmix64(uint64_t *state) {
	uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return z ^ (z >> 31);
}

void jc_engine_init(void) {
	if (s_tables_ready) {
		return;
	}
	uint64_t state = JC_ZOBRIST_SEED;
	for (int piece = 0; piece < JC_PIECE_KEYS; piece++) {
		for (int square = 0; square < JC_BOARD_SLOTS; square++) {
			s_piece_key[piece][square] = splitmix64(&state);
		}
	}
	s_side_key = splitmix64(&state);
	for (int rights = 0; rights <= JC_CASTLE_ALL; rights++) {
		s_castle_key[rights] = splitmix64(&state);
	}
	for (int file = 0; file < 8; file++) {
		s_en_passant_key[file] = splitmix64(&state);
	}
	build_castle_masks();
	jc_attack_tables_init();
	s_tables_ready = true;
}

// ---------------------------------------------------------------------------
// Castling rights: which squares, when touched, put an end to which right
// ---------------------------------------------------------------------------
//
// A right ends when the king moves, when its rook moves, or when that rook is
// captured where it stands. All three are "a piece left or arrived at this
// square", so one table of masks — ANDed for both ends of every move — covers
// every case without a single branch.

static uint8_t s_castle_mask[JC_BOARD_SLOTS];

static void build_castle_masks(void) {
	memset(s_castle_mask, JC_CASTLE_ALL, sizeof s_castle_mask);
	s_castle_mask[JC_SQUARE(4, 0)] = (uint8_t)(JC_CASTLE_ALL & ~(JC_CASTLE_WHITE_KING | JC_CASTLE_WHITE_QUEEN));
	s_castle_mask[JC_SQUARE(7, 0)] = (uint8_t)(JC_CASTLE_ALL & ~JC_CASTLE_WHITE_KING);
	s_castle_mask[JC_SQUARE(0, 0)] = (uint8_t)(JC_CASTLE_ALL & ~JC_CASTLE_WHITE_QUEEN);
	s_castle_mask[JC_SQUARE(4, 7)] = (uint8_t)(JC_CASTLE_ALL & ~(JC_CASTLE_BLACK_KING | JC_CASTLE_BLACK_QUEEN));
	s_castle_mask[JC_SQUARE(7, 7)] = (uint8_t)(JC_CASTLE_ALL & ~JC_CASTLE_BLACK_KING);
	s_castle_mask[JC_SQUARE(0, 7)] = (uint8_t)(JC_CASTLE_ALL & ~JC_CASTLE_BLACK_QUEEN);
}

// ---------------------------------------------------------------------------
// The three writers: everything that touches the board goes through these
// ---------------------------------------------------------------------------

static void place(JcPosition *position, uint8_t square, uint8_t piece) {
	const uint8_t colour = JC_COLOUR_OF(piece);
	const uint8_t slot = position->roster_count[colour]++;
	position->board[square] = piece;
	position->roster[colour][slot] = square;
	position->roster_slot[square] = slot;
	position->key ^= s_piece_key[piece][square];
	if (JC_KIND(piece) == JC_KING) {
		position->king[colour] = square;
	}
}

static void lift(JcPosition *position, uint8_t square) {
	const uint8_t piece = position->board[square];
	const uint8_t colour = JC_COLOUR_OF(piece);
	const uint8_t slot = position->roster_slot[square];
	// Fill the hole with the roster's last entry, so the list stays packed.
	const uint8_t last = --position->roster_count[colour];
	const uint8_t moved = position->roster[colour][last];
	position->roster[colour][slot] = moved;
	position->roster_slot[moved] = slot;
	position->board[square] = JC_EMPTY;
	position->key ^= s_piece_key[piece][square];
}

static void shift(JcPosition *position, uint8_t from, uint8_t to) {
	const uint8_t piece = position->board[from];
	const uint8_t colour = JC_COLOUR_OF(piece);
	const uint8_t slot = position->roster_slot[from];
	position->roster[colour][slot] = to;
	position->roster_slot[to] = slot;
	position->board[to] = piece;
	position->board[from] = JC_EMPTY;
	position->key ^= s_piece_key[piece][from] ^ s_piece_key[piece][to];
	if (JC_KIND(piece) == JC_KING) {
		position->king[colour] = to;
	}
}

// ---------------------------------------------------------------------------
// En passant: only when it is really on offer
// ---------------------------------------------------------------------------

// Could a pawn of `by` capture onto `target` right now? A double push sets the
// en-passant square only when this is true (see the file's banner).
static bool en_passant_is_available(const JcPosition *position, uint8_t target, uint8_t by) {
	const int back = -JC_FORWARD(by); // from the target, back toward the capturer
	const uint8_t pawn = JC_PIECE(by, JC_PAWN);
	for (int side_step = -1; side_step <= 1; side_step += 2) {
		const int square = (int)target + back + side_step;
		if (JC_ON_BOARD(square) && position->board[square] == pawn) {
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// Setting a position up
// ---------------------------------------------------------------------------

// The hash, computed from scratch. `jc_make` keeps the key up to date
// incrementally; this is what that incremental work must always agree with.
static uint64_t hash_position(const JcPosition *position) {
	uint64_t key = 0;
	for (uint8_t colour = 0; colour < JC_COLOURS; colour++) {
		for (uint8_t slot = 0; slot < position->roster_count[colour]; slot++) {
			const uint8_t square = position->roster[colour][slot];
			key ^= s_piece_key[position->board[square]][square];
		}
	}
	key ^= s_castle_key[position->castling];
	if (position->en_passant != JC_NO_SQUARE) {
		key ^= s_en_passant_key[JC_FILE_OF(position->en_passant)];
	}
	if (position->side == JC_BLACK) {
		key ^= s_side_key;
	}
	return key;
}

// Start from an empty board with no rights and nobody's turn — the state every
// setup path fills in.
static void clear_position(JcPosition *position) {
	memset(position, 0, sizeof *position);
	position->en_passant = JC_NO_SQUARE;
	position->king[JC_WHITE] = JC_NO_SQUARE;
	position->king[JC_BLACK] = JC_NO_SQUARE;
	position->fullmove = 1;
}

void jc_position_start(JcPosition *position) {
	static const char *START =
	    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
	jc_position_from_fen(position, START);
}

// The letter each kind is written with, indexed by kind. Uppercase is White's;
// notation.c lowercases it for Black.
static const char JC_KIND_LETTER[JC_KINDS] = {'.', 'P', 'N', 'B', 'R', 'Q', 'K'};

static uint8_t kind_from_letter(char letter) {
	for (uint8_t kind = JC_PAWN; kind < JC_KINDS; kind++) {
		if (JC_KIND_LETTER[kind] == letter) {
			return kind;
		}
	}
	return JC_EMPTY;
}

static bool is_upper(char c) {
	return c >= 'A' && c <= 'Z';
}

static char to_upper(char c) {
	return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static int read_number(const char **text) {
	int value = 0;
	while (**text >= '0' && **text <= '9') {
		value = value * 10 + (**text - '0');
		(*text)++;
	}
	return value;
}

static void skip_spaces(const char **text) {
	while (**text == ' ') {
		(*text)++;
	}
}

bool jc_position_from_fen(JcPosition *position, const char *fen) {
	JcPosition built;
	clear_position(&built);

	// 1. The array, written from the eighth rank down, files left to right.
	int file = 0, rank = 7;
	const char *text = fen;
	for (; *text && *text != ' '; text++) {
		if (*text == '/') {
			if (file != 8 || rank == 0) {
				return false;
			}
			file = 0;
			rank--;
		} else if (*text >= '1' && *text <= '8') {
			file += *text - '0';
		} else {
			const uint8_t kind = kind_from_letter(to_upper(*text));
			if (kind == JC_EMPTY || file > 7 || rank < 0) {
				return false;
			}
			place(&built, JC_SQUARE(file, rank), JC_PIECE(is_upper(*text) ? JC_WHITE : JC_BLACK, kind));
			file++;
		}
	}
	if (file != 8 || rank != 0) {
		return false;
	}

	// 2. Whose turn.
	skip_spaces(&text);
	if (*text != 'w' && *text != 'b') {
		return false;
	}
	built.side = (*text == 'b') ? JC_BLACK : JC_WHITE;
	text++;

	// 3. The castling rights.
	skip_spaces(&text);
	for (; *text && *text != ' '; text++) {
		switch (*text) {
		case 'K': built.castling |= JC_CASTLE_WHITE_KING; break;
		case 'Q': built.castling |= JC_CASTLE_WHITE_QUEEN; break;
		case 'k': built.castling |= JC_CASTLE_BLACK_KING; break;
		case 'q': built.castling |= JC_CASTLE_BLACK_QUEEN; break;
		case '-': break;
		default: return false;
		}
	}

	// 4. The en-passant square — kept only if the capture is really on offer.
	skip_spaces(&text);
	if (*text != '-' && *text != '\0') {
		const int ep_file = *text++ - 'a';
		const int ep_rank = *text++ - '1';
		if (ep_file < 0 || ep_file > 7 || ep_rank < 0 || ep_rank > 7) {
			return false;
		}
		const uint8_t square = JC_SQUARE(ep_file, ep_rank);
		if (en_passant_is_available(&built, square, built.side)) {
			built.en_passant = square;
		}
	} else if (*text == '-') {
		text++;
	}

	// 5. The two clocks, both optional.
	skip_spaces(&text);
	built.halfmove = (uint16_t)read_number(&text);
	skip_spaces(&text);
	const int fullmove = read_number(&text);
	built.fullmove = (uint16_t)(fullmove > 0 ? fullmove : 1);

	if (built.king[JC_WHITE] == JC_NO_SQUARE || built.king[JC_BLACK] == JC_NO_SQUARE) {
		return false;
	}
	built.key = hash_position(&built);
	*position = built;
	return true;
}

char *jc_position_to_fen(const JcPosition *position, char *out) {
	char *pen = out;
	for (int rank = 7; rank >= 0; rank--) {
		int empty = 0;
		for (int file = 0; file < 8; file++) {
			const uint8_t piece = position->board[JC_SQUARE(file, rank)];
			if (piece == JC_EMPTY) {
				empty++;
				continue;
			}
			if (empty) {
				*pen++ = (char)('0' + empty);
				empty = 0;
			}
			const char letter = JC_KIND_LETTER[JC_KIND(piece)];
			*pen++ = JC_COLOUR_OF(piece) == JC_WHITE ? letter : (char)(letter - 'A' + 'a');
		}
		if (empty) {
			*pen++ = (char)('0' + empty);
		}
		if (rank) {
			*pen++ = '/';
		}
	}
	*pen++ = ' ';
	*pen++ = position->side == JC_WHITE ? 'w' : 'b';
	*pen++ = ' ';
	if (position->castling == 0) {
		*pen++ = '-';
	} else {
		static const char RIGHTS[] = "KQkq";
		for (int bit = 0; bit < 4; bit++) {
			if (position->castling & (1 << bit)) {
				*pen++ = RIGHTS[bit];
			}
		}
	}
	*pen++ = ' ';
	if (position->en_passant == JC_NO_SQUARE) {
		*pen++ = '-';
	} else {
		pen = jc_square_name(position->en_passant, pen) + 2;
	}
	// The two clocks. Neither can reach three digits in a legal game: the
	// fifty-move rule ends one at 100 plies, and no game has reached move 999.
	*pen++ = ' ';
	pen += jc_uint_to_text(position->halfmove, pen);
	*pen++ = ' ';
	pen += jc_uint_to_text(position->fullmove, pen);
	*pen = '\0';
	return out;
}

// ---------------------------------------------------------------------------
// Playing a move, and putting it back
// ---------------------------------------------------------------------------

// Where the rook starts and finishes when the king castles to `king_to`.
// The king's destination file says which side: g-file is the short castle,
// c-file the long one.
static void castle_rook_squares(uint8_t king_to, uint8_t *rook_from, uint8_t *rook_to) {
	const int rank = JC_RANK_OF(king_to);
	const bool short_side = JC_FILE_OF(king_to) == 6;
	*rook_from = JC_SQUARE(short_side ? 7 : 0, rank);
	*rook_to = JC_SQUARE(short_side ? 5 : 3, rank);
}

void jc_make(JcPosition *position, JcMove move, JcUndo *undo) {
	const uint8_t colour = position->side;
	const uint8_t piece = position->board[move.from];

	undo->captured = JC_EMPTY;
	undo->castling = position->castling;
	undo->en_passant = position->en_passant;
	undo->halfmove = position->halfmove;
	undo->key = position->key;

	// Fold the old rights and en-passant file out of the hash; the new ones
	// are folded back in at the end.
	position->key ^= s_castle_key[position->castling];
	if (position->en_passant != JC_NO_SQUARE) {
		position->key ^= s_en_passant_key[JC_FILE_OF(position->en_passant)];
	}

	position->halfmove++;

	if (move.flags & JC_MOVE_EN_PASSANT) {
		// The victim stands beside the target square, not on it.
		const uint8_t victim = (uint8_t)((int)move.to - JC_FORWARD(colour));
		undo->captured = position->board[victim];
		lift(position, victim);
		position->halfmove = 0;
	} else if (move.flags & JC_MOVE_CAPTURE) {
		undo->captured = position->board[move.to];
		lift(position, move.to);
		position->halfmove = 0;
	}

	shift(position, move.from, move.to);
	if (JC_KIND(piece) == JC_PAWN) {
		position->halfmove = 0;
	}

	if (move.flags & JC_MOVE_PROMOTION) {
		lift(position, move.to);
		place(position, move.to, JC_PIECE(colour, move.promotion));
	}

	if (move.flags & JC_MOVE_CASTLE) {
		uint8_t rook_from, rook_to;
		castle_rook_squares(move.to, &rook_from, &rook_to);
		shift(position, rook_from, rook_to);
	}

	position->en_passant = JC_NO_SQUARE;
	if (move.flags & JC_MOVE_DOUBLE_PUSH) {
		const uint8_t skipped = (uint8_t)((int)move.from + JC_FORWARD(colour));
		if (en_passant_is_available(position, skipped, JC_OPPONENT(colour))) {
			position->en_passant = skipped;
		}
	}

	position->castling &= s_castle_mask[move.from] & s_castle_mask[move.to];
	position->key ^= s_castle_key[position->castling];
	if (position->en_passant != JC_NO_SQUARE) {
		position->key ^= s_en_passant_key[JC_FILE_OF(position->en_passant)];
	}

	if (colour == JC_BLACK) {
		position->fullmove++;
	}
	position->side = JC_OPPONENT(colour);
	position->key ^= s_side_key;
}

void jc_unmake(JcPosition *position, JcMove move, const JcUndo *undo) {
	position->side = JC_OPPONENT(position->side);
	const uint8_t colour = position->side;
	if (colour == JC_BLACK) {
		position->fullmove--;
	}

	if (move.flags & JC_MOVE_CASTLE) {
		uint8_t rook_from, rook_to;
		castle_rook_squares(move.to, &rook_from, &rook_to);
		shift(position, rook_to, rook_from);
	}

	shift(position, move.to, move.from);
	if (move.flags & JC_MOVE_PROMOTION) {
		lift(position, move.from);
		place(position, move.from, JC_PIECE(colour, JC_PAWN));
	}

	if (move.flags & JC_MOVE_EN_PASSANT) {
		place(position, (uint8_t)((int)move.to - JC_FORWARD(colour)), undo->captured);
	} else if (move.flags & JC_MOVE_CAPTURE) {
		place(position, move.to, undo->captured);
	}

	position->castling = undo->castling;
	position->en_passant = undo->en_passant;
	position->halfmove = undo->halfmove;
	// The three writers above kept a running key for a journey we are undoing;
	// the key we want is simply the one we started from.
	position->key = undo->key;
}

bool jc_make_if_legal(JcPosition *position, JcMove move, JcUndo *undo) {
	const uint8_t mover = position->side;
	jc_make(position, move, undo);
	if (jc_is_attacked(position, position->king[mover], position->side)) {
		jc_unmake(position, move, undo);
		return false;
	}
	return true;
}

void jc_make_null(JcPosition *position, JcUndo *undo) {
	undo->captured = JC_EMPTY;
	undo->castling = position->castling;
	undo->en_passant = position->en_passant;
	undo->halfmove = position->halfmove;
	undo->key = position->key;

	if (position->en_passant != JC_NO_SQUARE) {
		position->key ^= s_en_passant_key[JC_FILE_OF(position->en_passant)];
		position->en_passant = JC_NO_SQUARE;
	}
	position->halfmove++;
	position->side = JC_OPPONENT(position->side);
	position->key ^= s_side_key;
}

void jc_unmake_null(JcPosition *position, const JcUndo *undo) {
	position->side = JC_OPPONENT(position->side);
	position->en_passant = undo->en_passant;
	position->halfmove = undo->halfmove;
	position->key = undo->key;
}

// ---------------------------------------------------------------------------
// The invariant
// ---------------------------------------------------------------------------

bool jc_position_is_sound(const JcPosition *position) {
	int seen[JC_COLOURS] = {0, 0};
	for (int square = 0; square < JC_BOARD_SLOTS; square++) {
		if (!JC_ON_BOARD(square)) {
			continue;
		}
		const uint8_t piece = position->board[square];
		if (piece == JC_EMPTY) {
			continue;
		}
		const uint8_t colour = JC_COLOUR_OF(piece);
		const uint8_t slot = position->roster_slot[square];
		if (slot >= position->roster_count[colour] || position->roster[colour][slot] != square) {
			return false;
		}
		seen[colour]++;
	}
	if (seen[JC_WHITE] != position->roster_count[JC_WHITE] ||
	    seen[JC_BLACK] != position->roster_count[JC_BLACK]) {
		return false;
	}
	for (uint8_t colour = 0; colour < JC_COLOURS; colour++) {
		const uint8_t king = position->king[colour];
		if (!JC_ON_BOARD(king) || position->board[king] != JC_PIECE(colour, JC_KING)) {
			return false;
		}
	}
	return position->key == hash_position(position);
}
