// carts/jetchess/engine/book.h
//
// The opening book: a few hundred positions the engine does not have to think
// about, and the names of the openings they belong to.
//
// It does two jobs at once, and they are the same data read two ways.
//
//   IT PLAYS. In the first few moves a search is nearly useless — every
//   sensible move looks the same to an evaluation, and the ideas that matter
//   are twenty plies out of reach. A book replaces that with a century of
//   other people's judgement, answers instantly, and — because the book knows
//   several replies to most positions and chooses among them by weight — means
//   no two games start the same way.
//
//   IT NAMES. Every line ends in a position with a name, so the moment a game
//   reaches one, the board can say "Ruy López" or "Sicilian Defence: Najdorf".
//   That works for the player's moves as readily as the engine's, and it keeps
//   working after the book runs out, because the last name found stands until
//   another replaces it.
#ifndef JETCHESS_ENGINE_BOOK_H
#define JETCHESS_ENGINE_BOOK_H

#include "engine/chess.h"

// Replay every line in the book and build the two tables. Call once, after
// jc_engine_init. Returns the number of lines that could not be replayed,
// which is zero in a book that is correct — the tests hold it to that.
int jc_book_init(void);

// A book reply to this position, chosen among those the book knows by their
// weights, or a none-move when the book has nothing to say. `seed` is the
// caller's own generator state, advanced on each call.
JcMove jc_book_move(const JcPosition *position, uint32_t *seed);

// The name of the opening this exact position IS, or NULL. A caller showing
// the name keeps the last one it was given, because a game that leaves the
// book is still the opening it came from.
const char *jc_book_name(uint64_t key);

// How many positions the book knows, and how many names it holds — for the
// tests, and for anybody curious enough to look.
int jc_book_position_count(void);
int jc_book_name_count(void);

#endif // JETCHESS_ENGINE_BOOK_H
