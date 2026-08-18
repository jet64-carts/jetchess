// carts/jetchess/save.c
//
// The record, its format, and the two moments it is read and written (save.h).
//
// THE FILE. One file, `game.sav`, in the cartridge's own AppData folder — the
// only place a cart may write (jet_io.h). Every number in it is little-endian,
// and every field is laid down by hand rather than by copying a struct out,
// because a struct's padding and byte order are the compiler's business and a
// save file's layout is not:
//
//    0  4    the bytes 'J', 'C', 'S', '1'
//    4  u32  checksum, FNV-1a over every byte from 8 to the end
//    8  u16  version
//   10  u16  plies recorded
//   12  u32  the game's thread through the opening book
//   16  u8   the colour the player has
//   17  u8   the opponent's level
//   18  u8   settings: bit 0 the board is turned round, bit 1 the rate is shown
//   19  u8   spare, zero
//   20  ...  three bytes a move: from, to, and what a pawn became
//
// A WRITE IS THE WHOLE RECORD, never a patch of one, and that is what makes a
// failed write cost nothing but the moment: the next one to succeed carries
// everything the missed one would have. It is also why there is no write to a
// second file and a rename — the atomic dance buys nothing that the magic and
// the checksum do not already buy, and rename over an existing file is not
// portable. A torn file fails its checksum and reads as no file at all, which
// is the one honest thing it can mean.
#include "save.h"

#include "config.h"
#include "jet_io.h"

// Where it is kept. The AppData folder is the cart's own, so the record sits
// at its root and no directory has to be made first.
#define JC_SAVE_PATH "game.sav"

// The four bytes a record names itself with, before it claims anything, and
// the shape it claims to be.
static const uint8_t JC_SAVE_MAGIC[4] = {'J', 'C', 'S', '1'};
#define JC_SAVE_VERSION 1

// Every field's place in the record. The sum the checksum covers starts after
// the checksum itself, so it vouches for the version, the counts, the seed,
// the settings and every move — everything a reader acts on.
enum {
	JC_SAVE_AT_MAGIC = 0,
	JC_SAVE_AT_SUM = 4,
	JC_SAVE_AT_VERSION = 8,
	JC_SAVE_AT_PLIES = 10,
	JC_SAVE_AT_SEED = 12,
	JC_SAVE_AT_COLOUR = 16,
	JC_SAVE_AT_LEVEL = 17,
	JC_SAVE_AT_SETTINGS = 18,
	JC_SAVE_AT_SPARE = 19,
	JC_SAVE_AT_MOVES = 20,
};

// The two settings the hand keeps, as bits of the settings byte.
#define JC_SAVE_FLIPPED (1u << 0)
#define JC_SAVE_SHOWING_RATE (1u << 1)

// The map above and the size save.h reserves are the same statement made
// twice, so the compiler is asked to hold them together.
_Static_assert(JC_SAVE_AT_MOVES == JC_SAVE_PREAMBLE_BYTES,
               "the move list must start where save.h says the preamble ends");

// ---------------------------------------------------------------------------
// Bytes
// ---------------------------------------------------------------------------

static void put_u16(uint8_t *out, unsigned value) {
	out[0] = (uint8_t)(value & 0xFFu);
	out[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void put_u32(uint8_t *out, uint32_t value) {
	out[0] = (uint8_t)(value & 0xFFu);
	out[1] = (uint8_t)((value >> 8) & 0xFFu);
	out[2] = (uint8_t)((value >> 16) & 0xFFu);
	out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static unsigned take_u16(const uint8_t *in) {
	return (unsigned)in[0] | ((unsigned)in[1] << 8);
}

static uint32_t take_u32(const uint8_t *in) {
	return (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) |
	       ((uint32_t)in[3] << 24);
}

// FNV-1a over the part of the record the checksum vouches for. It is here to
// catch a file cut short or a byte gone bad, which is all a save file's guard
// has ever had to do; it is not a defence against anyone editing one on
// purpose, and nothing downstream treats it as one.
static uint32_t checksum(const uint8_t *bytes, int length) {
	uint32_t hash = 2166136261u;
	for (int at = JC_SAVE_AT_VERSION; at < length; at++) {
		hash ^= bytes[at];
		hash *= 16777619u;
	}
	return hash;
}

// ---------------------------------------------------------------------------
// The file
// ---------------------------------------------------------------------------

// The whole file, up to what a record can be, into `bytes`. Answers how many
// bytes came back; 0 when there is no file to read.
static int read_record(uint8_t *bytes) {
	JET_FILE file = jet_fopen(JC_SAVE_PATH, "rb");
	if (file == NULL) {
		return 0;
	}
	const int length = (int)jet_fread(bytes, 1, (size_t)JC_SAVE_BYTES, file);
	jet_fclose(file);
	return length;
}

// `length` bytes over whatever was there. Nothing is reported: persistence
// must never interrupt a game, and a write that did not happen is repaired by
// the next one, which carries the same whole record (see the file's head).
static void write_record(const uint8_t *bytes, int length) {
	JET_FILE file = jet_fopen(JC_SAVE_PATH, "wb");
	if (file == NULL) {
		return;
	}
	jet_fwrite(bytes, (size_t)length, 1, file);
	jet_fclose(file);
}

// ---------------------------------------------------------------------------
// The record
// ---------------------------------------------------------------------------

// Write the whole record into `out`, which must hold JC_SAVE_BYTES, and answer
// how long it is.
static int encode(uint8_t *out, const JcView *view) {
	const JcMatch *match = view->match;
	const int plies = match->ply_count;
	const int length = JC_SAVE_AT_MOVES + plies * JC_SAVE_MOVE_BYTES;

	memcpy(out + JC_SAVE_AT_MAGIC, JC_SAVE_MAGIC, sizeof JC_SAVE_MAGIC);
	put_u16(out + JC_SAVE_AT_VERSION, JC_SAVE_VERSION);
	put_u16(out + JC_SAVE_AT_PLIES, (unsigned)plies);
	put_u32(out + JC_SAVE_AT_SEED, match->book_seed);
	out[JC_SAVE_AT_COLOUR] = match->human_colour;
	out[JC_SAVE_AT_LEVEL] = (uint8_t)view->brain->level;
	out[JC_SAVE_AT_SETTINGS] =
	    (uint8_t)((view->hand->flipped ? JC_SAVE_FLIPPED : 0u) |
	              (view->hand->showing_rate ? JC_SAVE_SHOWING_RATE : 0u));
	out[JC_SAVE_AT_SPARE] = 0;

	uint8_t *ply = out + JC_SAVE_AT_MOVES;
	for (int at = 0; at < plies; at++, ply += JC_SAVE_MOVE_BYTES) {
		const JcMove move = match->played[at].move;
		ply[0] = move.from;
		ply[1] = move.to;
		ply[2] = move.promotion;
	}

	put_u32(out + JC_SAVE_AT_SUM, checksum(out, length));
	return length;
}

// Is `length` bytes at `bytes` a record this version can act on? Everything a
// reader will go on to trust is settled here: that it is one of ours, that it
// is a version we know, that its length is exactly the one its own ply count
// asks for, and that the bytes are the bytes that were written.
static bool record_is_sound(const uint8_t *bytes, int length) {
	if (length < JC_SAVE_AT_MOVES) {
		return false;
	}
	if (memcmp(bytes + JC_SAVE_AT_MAGIC, JC_SAVE_MAGIC, sizeof JC_SAVE_MAGIC) != 0) {
		return false;
	}
	if (take_u16(bytes + JC_SAVE_AT_VERSION) != JC_SAVE_VERSION) {
		return false;
	}
	const unsigned plies = take_u16(bytes + JC_SAVE_AT_PLIES);
	if (plies > (unsigned)JC_MATCH_MAX_PLIES) {
		return false;
	}
	if (length != JC_SAVE_AT_MOVES + (int)plies * JC_SAVE_MOVE_BYTES) {
		return false;
	}
	return take_u32(bytes + JC_SAVE_AT_SUM) == checksum(bytes, length);
}

// ---------------------------------------------------------------------------
// The two moments
// ---------------------------------------------------------------------------

bool jc_save_restore(JcSave *save, JcMatch *match, JcBrain *brain, JcHand *hand) {
	save->length = 0; // until a record is proved, nothing is known about the file

	const int length = read_record(save->draft);
	if (!record_is_sound(save->draft, length)) {
		return false;
	}
	const uint8_t *bytes = save->draft;

	// The two choices that outlive a game. Both are clamped rather than
	// trusted: the record's checksum says the bytes are the bytes that were
	// written, not that whoever wrote them agreed with this build about how
	// many levels there are.
	const int level = bytes[JC_SAVE_AT_LEVEL];
	brain->level = level < JC_LEVEL_COUNT ? level : JC_LEVEL_DEFAULT;
	hand->flipped = (bytes[JC_SAVE_AT_SETTINGS] & JC_SAVE_FLIPPED) != 0;
	hand->showing_rate = (bytes[JC_SAVE_AT_SETTINGS] & JC_SAVE_SHOWING_RATE) != 0;

	// The game, replayed. Each move is looked up in the position it is played
	// into, so an illegal one — a record written before a rule was fixed — is
	// found rather than forced, and the legal part of the game that reached it
	// is what the player gets back. The next sync writes that shorter game
	// down, so a record can heal but never rot.
	const uint8_t colour = bytes[JC_SAVE_AT_COLOUR] == JC_BLACK ? JC_BLACK : JC_WHITE;
	jc_match_new(match, colour, take_u32(bytes + JC_SAVE_AT_SEED));

	const int plies = (int)take_u16(bytes + JC_SAVE_AT_PLIES);
	const uint8_t *ply = bytes + JC_SAVE_AT_MOVES;
	int replayed = 0;
	for (; replayed < plies; replayed++, ply += JC_SAVE_MOVE_BYTES) {
		const JcMove move = jc_match_find_move(match, ply[0], ply[1], ply[2]);
		if (jc_move_is_none(move)) {
			break;
		}
		jc_match_play(match, move);
	}

	if (replayed == plies) {
		memcpy(save->image, save->draft, (size_t)length);
		save->length = length;
	}
	return true;
}

void jc_save_sync(JcSave *save, const JcView *view) {
	const int length = encode(save->draft, view);
	if (length == save->length && memcmp(save->draft, save->image, (size_t)length) == 0) {
		return;
	}
	write_record(save->draft, length);
	// Adopted whether or not the host took it. A folder that cannot be written
	// would otherwise be asked once a tick for ever; this way it is asked again
	// the next time the game actually changes, and that attempt carries
	// everything this one would have.
	memcpy(save->image, save->draft, (size_t)length);
	save->length = length;
}
