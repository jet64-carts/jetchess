// carts/jetchess/game/brain.h
//
// The opponent's turn: the one place that decides *when* the engine thinks and
// *who does the thinking*.
//
// On a machine with threads the search runs on a worker while the game keeps
// drawing at a hundred and twenty frames a second — the board never so much as
// ripples while the engine considers a move. In a browser there is no second
// thread, so the game loop does the search itself in slices of a few
// milliseconds a tick (engine/search.h). Both are the same search; the only
// difference is who turns the crank, and this file is the only one that knows
// which.
#ifndef JETCHESS_GAME_BRAIN_H
#define JETCHESS_GAME_BRAIN_H

#include "engine/search.h"
#include "game/match.h"
#include "jet_thread.h"

// A difficulty level, as a search setting and a name to show.
typedef struct {
	const char *name;
	const char *blurb; // one line, for the panel
	int depth;
	int spread;
	uint64_t cap_micros; // a safety net for a position that explodes
} JcLevel;

// The three levels, in order of strength. config.h names their indices.
extern const JcLevel JC_LEVEL_TABLE[];

typedef enum {
	JC_BRAIN_WAITING = 0, // it is not the engine's turn
	JC_BRAIN_THINKING,    // a search is under way
	JC_BRAIN_READY,       // a move is decided and waiting to be taken
} JcBrainState;

typedef struct {
	JcBrainState state;
	JcSearch search; // twenty-seven kilobytes: far too big for a stack frame

	// The worker, and the lock that publishes its answer. The lock is what
	// makes the finished flag safe to read: the worker sets it holding the
	// lock, and the game loop reads it holding the lock, so everything the
	// worker wrote to the search is visible once the flag is seen.
	bool threaded;
	jet_thread_t worker;
	jet_mutex_t lock;
	bool finished;   // the worker has written its answer
	bool abandoning; // ...and, the other way, that nobody wants it any more

	JcMove move;    // what it decided
	bool from_book; // ...and whether it thought at all
	int waited;     // ticks since it was asked, so a reply is never rude-quick
	int level;
} JcBrain;

// Set the brain up once, in _init. Reserves the transposition table from WRAM
// and works out whether this machine has a worker thread to give.
void jc_brain_init(JcBrain *brain, int level);

// Let it go, in _end: the worker is waited for and the lock destroyed.
void jc_brain_end(JcBrain *brain);

// Forget everything learned about the last game.
void jc_brain_new_game(JcBrain *brain);

// Begin thinking about `match`'s position. Does nothing if it is already
// thinking, or if the game is over.
void jc_brain_begin(JcBrain *brain, const JcMatch *match);

// Give the brain its slice of this tick — once per _update, which is what
// makes its own patience a count of ticks. Returns true the moment a move is
// ready; `jc_brain_take` then hands it over and returns the brain to waiting.
bool jc_brain_poll(JcBrain *brain);

// The decided move. Only meaningful after jc_brain_poll has returned true.
JcMove jc_brain_take(JcBrain *brain);

// Stop whatever is under way — a new game, or a move taken back, while the
// engine was thinking. Waits for the worker if there is one.
void jc_brain_abandon(JcBrain *brain);

// How deep it has got, how many positions it has looked at, and how long it
// has been thinking: the panel's readout.
static inline int jc_brain_depth(const JcBrain *brain) {
	return brain->search.result.depth;
}
static inline uint64_t jc_brain_nodes(const JcBrain *brain) {
	return brain->search.result.nodes;
}

#endif // JETCHESS_GAME_BRAIN_H
