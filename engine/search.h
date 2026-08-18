// carts/jetchess/engine/search.h
//
// Finding a move. The search is written so it can be driven two ways from one
// body of code, because the machine JetChess runs on may or may not have a
// second thread:
//
//   * On a desktop, a worker thread calls `jc_search_run`, which deepens until
//     it is done while the game keeps drawing at full rate.
//   * In a browser, where there is no second thread, the game loop calls
//     `jc_search_step` once a tick — one deepening iteration each time — and
//     the answer improves over a few frames instead of arriving at once.
//
// Both drivers turn the same crank. There is no second search and no `#ifdef`
// anywhere in this file: the difference is only who calls, and how often.
#ifndef JETCHESS_ENGINE_SEARCH_H
#define JETCHESS_ENGINE_SEARCH_H

#include "engine/chess.h"

// The deepest line the search will follow. Sixty-four plies is thirty-two
// moves each — far past anything a real search reaches, and the bound that
// makes every per-ply array a fixed size.
#define JC_MAX_PLY 64

// How many earlier positions the search is told about, so it can see a
// repetition coming. One per ply of the game so far, capped: a game longer
// than this has long since passed any position that could still repeat.
#define JC_HISTORY_KEYS 256

// A score no evaluation can reach, used as the opening window.
#define JC_INFINITY 32000

// Mate is scored just under infinity and counted down by ply, so that a mate
// in two outscores a mate in three and the search hurries.
#define JC_MATE_SCORE 30000
#define JC_MATE_THRESHOLD (JC_MATE_SCORE - JC_MAX_PLY)

// True when a score means "somebody is getting mated", either way round.
static inline bool jc_score_is_mate(int score) {
	return score > JC_MATE_THRESHOLD || score < -JC_MATE_THRESHOLD;
}

// How many moves away that mate is, for a score that is one.
static inline int jc_mate_in_moves(int score) {
	const int plies = JC_MATE_SCORE - (score > 0 ? score : -score);
	return (plies + 1) / 2;
}

// ---------------------------------------------------------------------------
// What the caller asks for
// ---------------------------------------------------------------------------

typedef struct {
	int depth;           // the deepest iteration to complete
	uint64_t micros;     // give up deepening past this much time; 0 means never
	uint64_t step_micros; // how long one jc_search_step may run; 0 means as long as it needs
	int spread;          // see below
} JcSearchLimits;

// `step_micros` is what makes a browser playable. With no second thread the
// game loop has to do the searching itself, between drawing frames, so a step
// that ran a whole deepening iteration would freeze the picture for as long as
// that iteration took. Instead a step stops at the next root move once its
// budget is spent and picks up there next tick: the iteration spans a handful
// of frames and none of them is late. A worker thread passes 0 and simply runs
// to the end, which is the same code with the budget switched off.

// `spread` is how the difficulty levels are made, and it is deliberately NOT a
// handicap the search plays worse to achieve. The search always plays its best;
// `spread` only decides which of the moves it *likes* gets chosen. At 0 the
// best move is played every time. At 40 any root move within forty centipawns
// of the best — a couple of tempi — may be chosen instead. That makes a weaker
// and far more human opponent than a crippled search: it still never hangs a
// queen, but it will choose the second-best plan, and no two games are alike.

// ---------------------------------------------------------------------------
// What the caller gets back
// ---------------------------------------------------------------------------

typedef struct {
	JcMove best;      // the move to play
	int score;        // centipawns, from the searching side's point of view
	int depth;        // the deepest iteration that actually finished
	uint64_t nodes;   // positions looked at, for the readout
	uint64_t micros;  // how long it took
	JcMove line[JC_MAX_PLY]; // the principal variation: best play for both sides
	int line_length;
} JcSearchResult;

// ---------------------------------------------------------------------------
// The search's own state
// ---------------------------------------------------------------------------
//
// One of these is the whole search. It is a plain value with no pointers out
// of it except the shared table, so handing it to a worker thread is handing
// over one object and nothing else.

typedef struct {
	JcPosition position;
	JcSearchLimits limits;
	JcSearchResult result;

	int next_depth;  // the iteration to run when jc_search_step is next called
	bool done;       // no more iterations are worth running
	bool aborted;    // the clock ran out inside an iteration
	uint64_t deadline;

	// Where a part-finished iteration got to, so the next step resumes rather
	// than restarts. An iteration's scores are only believed once every root
	// move has been searched at that depth.
	bool iterating;
	int root_index;
	int root_alpha;
	int root_best;
	int root_partial[JC_MAX_MOVES];

	// The game's own history, plus the line being searched, so a repetition
	// is seen whether it is made of played moves or searched ones.
	uint64_t history[JC_HISTORY_KEYS + JC_MAX_PLY + 4];
	int history_count;
	int root_ply;

	// Move ordering, kept between iterations because a move that was good at
	// depth four is usually good at depth five.
	JcMove killers[JC_MAX_PLY][2];
	int16_t quiet_history[16][JC_BOARD_SLOTS];

	JcMoveList root;                // the root moves, reordered each iteration
	int root_score[JC_MAX_MOVES];   // what the last finished iteration thought
	uint32_t choice_seed;           // picks among equal-enough moves, for variety

	JcMove line[JC_MAX_PLY][JC_MAX_PLY]; // the principal variation, gathered per ply
	int line_length[JC_MAX_PLY];
} JcSearch;

// ---------------------------------------------------------------------------
// The shared transposition table
// ---------------------------------------------------------------------------

// Hand the table its memory once, at startup. `bytes` is rounded down to a
// power-of-two count of entries, because indexing by a mask is a great deal
// quicker than indexing by a remainder. Passing NULL, or too little memory,
// leaves the search working correctly and rather more slowly.
void jc_table_init(void *memory, unsigned bytes);

// Forget everything. Called at the start of each game so one game's scores
// can never leak into the next.
void jc_table_clear(void);

// How full the table is, in parts per thousand — the "hashfull" an engine
// readout traditionally shows.
int jc_table_load(void);

// ---------------------------------------------------------------------------
// Driving the search
// ---------------------------------------------------------------------------

// Set a search up over a copy of `position`. `history` is the keys of the
// positions already played, most recent last, so repetitions are seen.
void jc_search_begin(JcSearch *search, const JcPosition *position,
                     const uint64_t *history, int history_count, JcSearchLimits limits);

// Run one deepening iteration. Returns true when the search is finished and
// there is nothing left to do. Safe to call again after that: it does nothing.
bool jc_search_step(JcSearch *search);

// Step until finished. This is a worker thread's entire job.
void jc_search_run(JcSearch *search);

// ---------------------------------------------------------------------------
// The clock the search reads
// ---------------------------------------------------------------------------

// Supplied by whoever is hosting the engine: the cart answers with the
// machine's microsecond clock, and the test harness with its own. Declaring it
// here rather than calling jet_time_micros keeps engine/ pure C that builds and
// runs anywhere, which is what lets the tests hammer it outside the console.
uint64_t jc_clock_micros(void);

// Asked now and then while a search runs — about once every few thousand
// positions, on the same schedule as the clock. Answer true and the search
// gives up at once and returns whatever it already had.
//
// This is how a search running on a worker thread is called off: the player
// starts a new game, the host raises its own flag, and the worker notices
// within a fraction of a millisecond instead of the game waiting out a search
// whose answer nobody wants. It is a question about the host, not about one
// search, because only one search runs at a time.
bool jc_search_should_stop(void);

#endif // JETCHESS_ENGINE_SEARCH_H
