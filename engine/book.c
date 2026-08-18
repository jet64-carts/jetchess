// carts/jetchess/engine/book.c
//
// The book, as one table of lines (book.h says why there is a book at all).
//
// HOW A LINE IS WRITTEN. Each row is a weight, a name, and the moves in
// ordinary notation, exactly as a chess book prints them. At startup every row
// is replayed from the starting position; each move along the way is filed
// under the hash of the position it was played in, and the name is filed under
// the hash of the position the line ends on.
//
// The moves are read by *writing* — jc_move_from_san generates the legal moves
// and matches the one whose own notation agrees (notation.c). So a typing slip
// in this table cannot become a wrong move: it becomes a line that fails to
// replay, and `jc_book_init` counts those, and the tests insist the count is
// zero.
//
// WHAT THE WEIGHT MEANS. How willing the engine is to play the line, from 10
// (a main line: play it often) down to 1 (offbeat: once in a while, for the
// surprise). A weight of 0 is a line the book will NAME but never choose —
// there to recognise what the player has done, not to do it.
//
// Weights accumulate along shared beginnings, which is exactly right: 1.e4 is
// the first move of forty lines here and 1.b4 of one, so 1.e4 comes up about
// forty times as often. The book's opening move distribution is a consequence
// of how much of it is written, not a second table to keep in step.
#include "book.h"

// ---------------------------------------------------------------------------
// The lines
// ---------------------------------------------------------------------------

typedef struct {
	uint8_t weight;
	const char *name;
	const char *moves;
} JcBookLine;

#define JC_BOOK_MAIN 10
#define JC_BOOK_GOOD 6
#define JC_BOOK_SIDE 3
#define JC_BOOK_RARE 1
#define JC_BOOK_NAME_ONLY 0

static const JcBookLine JC_BOOK[] = {
	// ---- 1.e4: the open games ---------------------------------------------
	{JC_BOOK_MAIN, "King's Pawn Opening", "e4"},
	{JC_BOOK_MAIN, "King's Pawn Game", "e4 e5"},
	{JC_BOOK_MAIN, "King's Knight Opening", "e4 e5 Nf3"},
	{JC_BOOK_GOOD, "Petrov Defence", "e4 e5 Nf3 Nf6"},
	{JC_BOOK_SIDE, "Philidor Defence", "e4 e5 Nf3 d6"},
	{JC_BOOK_MAIN, "Open Game", "e4 e5 Nf3 Nc6"},
	{JC_BOOK_MAIN, "Italian Game", "e4 e5 Nf3 Nc6 Bc4"},
	{JC_BOOK_MAIN, "Giuoco Piano", "e4 e5 Nf3 Nc6 Bc4 Bc5"},
	{JC_BOOK_GOOD, "Giuoco Pianissimo", "e4 e5 Nf3 Nc6 Bc4 Bc5 d3 Nf6 O-O d6"},
	{JC_BOOK_GOOD, "Two Knights Defence", "e4 e5 Nf3 Nc6 Bc4 Nf6"},
	{JC_BOOK_SIDE, "Evans Gambit", "e4 e5 Nf3 Nc6 Bc4 Bc5 b4"},
	{JC_BOOK_MAIN, "Ruy Lopez", "e4 e5 Nf3 Nc6 Bb5"},
	{JC_BOOK_MAIN, "Ruy Lopez: Morphy Defence", "e4 e5 Nf3 Nc6 Bb5 a6"},
	{JC_BOOK_MAIN, "Ruy Lopez: Closed", "e4 e5 Nf3 Nc6 Bb5 a6 Ba4 Nf6 O-O Be7 Re1 b5 Bb3 d6 c3 O-O"},
	{JC_BOOK_GOOD, "Ruy Lopez: Berlin Defence", "e4 e5 Nf3 Nc6 Bb5 Nf6"},
	{JC_BOOK_SIDE, "Ruy Lopez: Exchange", "e4 e5 Nf3 Nc6 Bb5 a6 Bxc6 dxc6"},
	{JC_BOOK_GOOD, "Scotch Game", "e4 e5 Nf3 Nc6 d4"},
	{JC_BOOK_GOOD, "Scotch Game: Main Line", "e4 e5 Nf3 Nc6 d4 exd4 Nxd4"},
	{JC_BOOK_GOOD, "Four Knights Game", "e4 e5 Nf3 Nc6 Nc3 Nf6"},
	{JC_BOOK_SIDE, "Vienna Game", "e4 e5 Nc3"},
	{JC_BOOK_SIDE, "King's Gambit", "e4 e5 f4"},
	{JC_BOOK_NAME_ONLY, "King's Gambit Accepted", "e4 e5 f4 exf4"},
	{JC_BOOK_SIDE, "Bishop's Opening", "e4 e5 Bc4"},
	{JC_BOOK_RARE, "Centre Game", "e4 e5 d4 exd4 Qxd4 Nc6"},
	{JC_BOOK_RARE, "Ponziani Opening", "e4 e5 Nf3 Nc6 c3"},

	// ---- the Sicilian ------------------------------------------------------
	{JC_BOOK_MAIN, "Sicilian Defence", "e4 c5"},
	{JC_BOOK_MAIN, "Sicilian Defence: Open", "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3"},
	{JC_BOOK_MAIN, "Sicilian Defence: Najdorf", "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 a6"},
	{JC_BOOK_GOOD, "Sicilian Defence: Dragon", "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 g6"},
	{JC_BOOK_GOOD, "Sicilian Defence: Scheveningen", "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 e6"},
	{JC_BOOK_GOOD, "Sicilian Defence: Classical", "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 Nc6"},
	{JC_BOOK_GOOD, "Sicilian Defence: Sveshnikov", "e4 c5 Nf3 Nc6 d4 cxd4 Nxd4 Nf6 Nc3 e5"},
	{JC_BOOK_GOOD, "Sicilian Defence: Accelerated Dragon", "e4 c5 Nf3 Nc6 d4 cxd4 Nxd4 g6"},
	{JC_BOOK_GOOD, "Sicilian Defence: Taimanov", "e4 c5 Nf3 e6 d4 cxd4 Nxd4 Nc6"},
	{JC_BOOK_SIDE, "Sicilian Defence: Kan", "e4 c5 Nf3 e6 d4 cxd4 Nxd4 a6"},
	{JC_BOOK_GOOD, "Sicilian Defence: Alapin", "e4 c5 c3"},
	{JC_BOOK_SIDE, "Sicilian Defence: Closed", "e4 c5 Nc3 Nc6 g3"},
	{JC_BOOK_SIDE, "Sicilian Defence: Rossolimo", "e4 c5 Nf3 Nc6 Bb5"},
	{JC_BOOK_RARE, "Sicilian Defence: Smith-Morra Gambit", "e4 c5 d4 cxd4 c3"},

	// ---- the other answers to 1.e4 ----------------------------------------
	{JC_BOOK_MAIN, "French Defence", "e4 e6"},
	{JC_BOOK_GOOD, "French Defence: Advance", "e4 e6 d4 d5 e5 c5 c3 Nc6"},
	{JC_BOOK_SIDE, "French Defence: Exchange", "e4 e6 d4 d5 exd5 exd5"},
	{JC_BOOK_GOOD, "French Defence: Tarrasch", "e4 e6 d4 d5 Nd2"},
	{JC_BOOK_GOOD, "French Defence: Winawer", "e4 e6 d4 d5 Nc3 Bb4"},
	{JC_BOOK_GOOD, "French Defence: Classical", "e4 e6 d4 d5 Nc3 Nf6"},
	{JC_BOOK_MAIN, "Caro-Kann Defence", "e4 c6"},
	{JC_BOOK_GOOD, "Caro-Kann Defence: Advance", "e4 c6 d4 d5 e5 Bf5"},
	{JC_BOOK_GOOD, "Caro-Kann Defence: Classical", "e4 c6 d4 d5 Nc3 dxe4 Nxe4 Bf5"},
	{JC_BOOK_GOOD, "Caro-Kann Defence: Panov Attack", "e4 c6 d4 d5 exd5 cxd5 c4"},
	{JC_BOOK_SIDE, "Pirc Defence", "e4 d6 d4 Nf6 Nc3 g6"},
	{JC_BOOK_SIDE, "Modern Defence", "e4 g6 d4 Bg7"},
	{JC_BOOK_SIDE, "Scandinavian Defence", "e4 d5 exd5 Qxd5 Nc3 Qa5"},
	{JC_BOOK_SIDE, "Alekhine's Defence", "e4 Nf6 e5 Nd5 d4 d6"},
	{JC_BOOK_RARE, "Nimzowitsch Defence", "e4 Nc6"},

	// ---- 1.d4: the closed games -------------------------------------------
	{JC_BOOK_MAIN, "Queen's Pawn Opening", "d4"},
	{JC_BOOK_MAIN, "Queen's Pawn Game", "d4 d5"},
	{JC_BOOK_MAIN, "Queen's Gambit", "d4 d5 c4"},
	{JC_BOOK_GOOD, "Queen's Gambit Accepted", "d4 d5 c4 dxc4 Nf3 Nf6 e3"},
	{JC_BOOK_MAIN, "Queen's Gambit Declined", "d4 d5 c4 e6"},
	{JC_BOOK_GOOD, "Queen's Gambit Declined: Orthodox", "d4 d5 c4 e6 Nc3 Nf6 Bg5 Be7 e3 O-O Nf3 Nbd7"},
	{JC_BOOK_MAIN, "Slav Defence", "d4 d5 c4 c6"},
	{JC_BOOK_GOOD, "Semi-Slav Defence", "d4 d5 c4 c6 Nf3 Nf6 Nc3 e6"},
	{JC_BOOK_RARE, "Chigorin Defence", "d4 d5 c4 Nc6"},
	{JC_BOOK_GOOD, "London System", "d4 d5 Nf3 Nf6 Bf4 e6 e3 c5"},
	{JC_BOOK_SIDE, "Colle System", "d4 d5 Nf3 Nf6 e3 e6 Bd3"},
	{JC_BOOK_MAIN, "Indian Defence", "d4 Nf6"},
	{JC_BOOK_MAIN, "Nimzo-Indian Defence", "d4 Nf6 c4 e6 Nc3 Bb4"},
	{JC_BOOK_GOOD, "Queen's Indian Defence", "d4 Nf6 c4 e6 Nf3 b6"},
	{JC_BOOK_SIDE, "Bogo-Indian Defence", "d4 Nf6 c4 e6 Nf3 Bb4+"},
	{JC_BOOK_MAIN, "King's Indian Defence", "d4 Nf6 c4 g6 Nc3 Bg7"},
	{JC_BOOK_GOOD, "King's Indian Defence: Classical", "d4 Nf6 c4 g6 Nc3 Bg7 e4 d6 Nf3 O-O Be2 e5"},
	{JC_BOOK_GOOD, "Grunfeld Defence", "d4 Nf6 c4 g6 Nc3 d5"},
	{JC_BOOK_SIDE, "Benoni Defence", "d4 Nf6 c4 c5 d5"},
	{JC_BOOK_SIDE, "Modern Benoni", "d4 Nf6 c4 c5 d5 e6 Nc3 exd5 cxd5 d6"},
	{JC_BOOK_RARE, "Benko Gambit", "d4 Nf6 c4 c5 d5 b5"},
	{JC_BOOK_RARE, "Budapest Gambit", "d4 Nf6 c4 e5"},
	{JC_BOOK_SIDE, "Dutch Defence", "d4 f5"},
	{JC_BOOK_SIDE, "Dutch Defence: Leningrad", "d4 f5 g3 Nf6 Bg2 g6"},
	{JC_BOOK_SIDE, "Trompowsky Attack", "d4 Nf6 Bg5"},

	// ---- the flank openings ------------------------------------------------
	{JC_BOOK_GOOD, "English Opening", "c4"},
	{JC_BOOK_GOOD, "English Opening: Symmetrical", "c4 c5"},
	{JC_BOOK_GOOD, "English Opening: Reversed Sicilian", "c4 e5"},
	{JC_BOOK_SIDE, "Reti Opening", "Nf3 d5 c4"},
	{JC_BOOK_SIDE, "King's Indian Attack", "Nf3 d5 g3 Nf6 Bg2"},
	{JC_BOOK_RARE, "Bird's Opening", "f4"},
	{JC_BOOK_RARE, "Larsen's Opening", "b3"},
	{JC_BOOK_NAME_ONLY, "Sokolsky Opening", "b4"},
	{JC_BOOK_NAME_ONLY, "Grob's Attack", "g4"},
};

#define JC_BOOK_LINES ((int)(sizeof JC_BOOK / sizeof JC_BOOK[0]))

// ---------------------------------------------------------------------------
// The two tables the lines are replayed into
// ---------------------------------------------------------------------------

// Room for every ply of every line, with slack. Sized as a constant rather
// than grown, because a cart allocates nothing.
#define JC_BOOK_MAX_POSITIONS 1024
#define JC_BOOK_MAX_NAMES 256

typedef struct {
	uint64_t key;
	JcMove move;
	uint16_t weight;
} JcBookPosition;

typedef struct {
	uint64_t key;
	const char *name;
} JcBookName;

static JcBookPosition s_positions[JC_BOOK_MAX_POSITIONS];
static int s_position_count;
static JcBookName s_names[JC_BOOK_MAX_NAMES];
static int s_name_count;

int jc_book_position_count(void) {
	return s_position_count;
}

int jc_book_name_count(void) {
	return s_name_count;
}

// Split the next whitespace-separated word out of `text` into `word`, and
// answer where to carry on from. NULL when there is nothing left.
static const char *next_word(const char *text, char *word, int room) {
	while (*text == ' ') {
		text++;
	}
	if (*text == '\0') {
		return NULL;
	}
	int length = 0;
	while (*text != '\0' && *text != ' ') {
		if (length < room - 1) {
			word[length++] = *text;
		}
		text++;
	}
	word[length] = '\0';
	return text;
}

int jc_book_init(void) {
	s_position_count = 0;
	s_name_count = 0;
	int refused = 0;

	for (int line = 0; line < JC_BOOK_LINES; line++) {
		JcPosition position;
		jc_position_start(&position);
		const char *text = JC_BOOK[line].moves;
		char word[JC_SAN_LENGTH];
		bool replayed = true;

		while ((text = next_word(text, word, sizeof word)) != NULL) {
			const JcMove move = jc_move_from_san(&position, word);
			if (jc_move_is_none(move)) {
				replayed = false;
				break;
			}
			if (s_position_count < JC_BOOK_MAX_POSITIONS && JC_BOOK[line].weight > 0) {
				s_positions[s_position_count].key = position.key;
				s_positions[s_position_count].move = move;
				s_positions[s_position_count].weight = JC_BOOK[line].weight;
				s_position_count++;
			}
			JcUndo undo;
			jc_make(&position, move, &undo);
		}

		if (!replayed) {
			refused++;
			continue;
		}
		if (s_name_count < JC_BOOK_MAX_NAMES) {
			s_names[s_name_count].key = position.key;
			s_names[s_name_count].name = JC_BOOK[line].name;
			s_name_count++;
		}
	}
	return refused;
}

// ---------------------------------------------------------------------------
// Reading it back
// ---------------------------------------------------------------------------

// The same small generator the search uses to choose among equal moves.
static uint32_t next_random(uint32_t *state) {
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return *state;
}

JcMove jc_book_move(const JcPosition *position, uint32_t *seed) {
	// A straight walk of the table. It is a few hundred comparisons, once per
	// move of a game — an index would be more code and no faster than the
	// player can notice.
	unsigned total = 0;
	for (int i = 0; i < s_position_count; i++) {
		if (s_positions[i].key == position->key) {
			total += s_positions[i].weight;
		}
	}
	if (total == 0) {
		return jc_move_none();
	}
	unsigned wanted = next_random(seed) % total;
	for (int i = 0; i < s_position_count; i++) {
		if (s_positions[i].key != position->key) {
			continue;
		}
		if (wanted < s_positions[i].weight) {
			return s_positions[i].move;
		}
		wanted -= s_positions[i].weight;
	}
	return jc_move_none(); // unreachable: the weights were just counted
}

const char *jc_book_name(uint64_t key) {
	// Walked backwards, so a line written later — and therefore more specific,
	// since the table runs from the general to the particular — wins.
	for (int i = s_name_count - 1; i >= 0; i--) {
		if (s_names[i].key == key) {
			return s_names[i].name;
		}
	}
	return NULL;
}
