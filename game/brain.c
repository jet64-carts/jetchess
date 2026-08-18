// carts/jetchess/game/brain.c
//
// When the engine thinks, and who does the thinking (brain.h).
//
// THE HANDOVER. A worker thread and the game loop share one search. The rule
// that makes that safe is the ordinary one: the worker writes its results,
// then sets `finished` while holding the lock; the loop reads `finished` while
// holding the same lock, and only looks at the results once it has seen the
// flag. Taking a lock is what publishes everything written before it, so there
// is no other synchronisation to get right — and `jet_thread_join` is called
// before the answer is used, which settles it a second time.
//
// THE FLOOR. An engine that answers in four milliseconds does not feel like an
// opponent; it feels like a lookup table. So a reply is never delivered sooner
// than JC_THINK_FLOOR_TICKS after it was asked for, whether it came from the
// book or from a second of searching. The wait costs nothing — the game is
// drawing at full rate throughout — and it is what lets the move's sound and
// its slide across the board have room to be noticed.
#include "brain.h"

#include "config.h"
#include "engine/book.h"
#include "jet64.h"
#include "jet_host.h"
#include "jet_memory.h"
#include "jet_time.h"

// The clock engine/search.h asks the host for. Inside the console that is the
// machine's own microsecond counter.
uint64_t jc_clock_micros(void) {
	return jet_time_micros();
}

// The one brain, so the search can ask it whether to give up. There is exactly
// one search at a time in JetChess, which is what makes a single pointer the
// honest way to answer a question the search asks of "the host".
static JcBrain *s_brain;

bool jc_search_should_stop(void) {
	if (s_brain == NULL) {
		return false;
	}
	if (s_brain->lock == NULL) {
		return s_brain->abandoning; // no worker: nobody else can be writing it
	}
	jet_mutex_lock(s_brain->lock);
	const bool stop = s_brain->abandoning;
	jet_mutex_unlock(s_brain->lock);
	return stop;
}

// The three levels.
//
// THE DEPTHS come from what the search actually costs. From a middlegame
// position at the compiler's own -O3, measured by tests/jetchess: two plies
// 0.2 ms, four 5 ms, six 22 ms, eight 196 ms — so three plies is about a
// millisecond, five about ten, and even the strongest level answers well
// inside the third of a second it is made to wait out anyway. A browser,
// searching three milliseconds at a time between frames, reaches eight plies
// in a second or two without a single late frame.
//
// THE SPREADS come from playing them (tests/jetchess/strength.c), against a
// reference that cannot drift: the same search with the knob at zero. Casual
// sits about level with a plain two-ply search; Club between a plain three-ply
// and a plain four-ply one, which is the "challenging but beatable" the brief
// asked for; Master takes the knob off altogether.
const JcLevel JC_LEVEL_TABLE[JC_LEVEL_COUNT] = {
	{"CASUAL", "Three moves ahead, and easily tempted", 3, 90, 500000u},
	{"CLUB", "Five moves ahead, and rarely misses a tactic", 5, 50, 1500000u},
	{"MASTER", "Eight moves ahead, and plays the best it finds", 8, 0, 2500000u},
};

// The worker's whole job: run the search to the end, then say so.
static void *think(void *argument) {
	JcBrain *brain = (JcBrain *)argument;
	jc_search_run(&brain->search);
	jet_mutex_lock(brain->lock);
	brain->finished = true;
	jet_mutex_unlock(brain->lock);
	return NULL;
}

void jc_brain_init(JcBrain *brain, int level) {
	s_brain = brain;
	brain->abandoning = false;
	brain->state = JC_BRAIN_WAITING;
	brain->level = level;
	brain->finished = false;
	brain->worker = NULL;
	brain->move = jc_move_none();
	brain->from_book = false;
	brain->waited = 0;

	// A browser runs one thread, and there jet_thread_create would simply run
	// the search to the end before returning — freezing the picture for as
	// long as the search took. So the host is asked outright, and where the
	// answer is "no worker", the game loop searches in slices instead.
	brain->threaded = !jet_host_is_web();
	brain->lock = brain->threaded ? jet_mutex_create() : NULL;
	if (brain->lock == NULL) {
		brain->threaded = false;
	}

	// The transposition table comes out of WRAM, which may only be reserved
	// from _init. A refusal is not fatal: the search simply runs without a
	// table, more slowly and with the same answers.
	jc_table_init(jet_memory_reserve(JC_TABLE_BYTES), JC_TABLE_BYTES);
}

void jc_brain_end(JcBrain *brain) {
	jc_brain_abandon(brain);
	if (brain->lock) {
		jet_mutex_destroy(brain->lock);
		brain->lock = NULL;
	}
}

void jc_brain_new_game(JcBrain *brain) {
	jc_brain_abandon(brain);
	jc_table_clear();
}

void jc_brain_begin(JcBrain *brain, const JcMatch *match) {
	if (brain->state != JC_BRAIN_WAITING || match->result != JC_RESULT_PLAYING) {
		return;
	}
	brain->waited = 0;
	brain->finished = false;
	brain->abandoning = false;
	brain->move = jc_move_none();

	// The book first. It is a century of other people's judgement, it answers
	// in microseconds, and it is the only reason two games ever begin
	// differently (engine/book.h).
	uint32_t seed = match->book_seed;
	const JcMove opening_move = jc_book_move(&match->position, &seed);
	if (!jc_move_is_none(opening_move)) {
		brain->move = opening_move;
		brain->from_book = true;
		brain->state = JC_BRAIN_READY;
		return;
	}
	brain->from_book = false;

	const JcLevel *level = &JC_LEVEL_TABLE[brain->level];
	JcSearchLimits limits = {
		.depth = level->depth,
		.micros = level->cap_micros,
		// With a worker there is nothing to be gentle about: it runs flat out
		// on its own thread. Without one, every step has to give the frame
		// back in time to be drawn.
		.step_micros = brain->threaded ? 0 : JC_SEARCH_SLICE_MICROS,
		.spread = level->spread,
	};
	jc_search_begin(&brain->search, &match->position, match->key, match->ply_count + 1, limits);
	brain->state = JC_BRAIN_THINKING;

	if (brain->threaded) {
		brain->worker = jet_thread_create(think, brain);
		if (brain->worker == NULL) {
			// The machine would not give us a thread after all; carry on in
			// slices, which is correct everywhere and merely less smooth.
			brain->threaded = false;
			brain->search.limits.step_micros = JC_SEARCH_SLICE_MICROS;
		}
	}
}

// Has the search finished? On a worker that is a flag read under the lock; in
// slices it is this tick's slice returning "done".
static bool search_has_finished(JcBrain *brain) {
	if (!brain->threaded) {
		return jc_search_step(&brain->search);
	}
	jet_mutex_lock(brain->lock);
	const bool finished = brain->finished;
	jet_mutex_unlock(brain->lock);
	if (finished) {
		jet_thread_join(brain->worker);
		brain->worker = NULL;
	}
	return finished;
}

bool jc_brain_poll(JcBrain *brain) {
	if (brain->state == JC_BRAIN_WAITING) {
		return false;
	}
	brain->waited++;
	if (brain->state == JC_BRAIN_THINKING && search_has_finished(brain)) {
		brain->move = brain->search.result.best;
		brain->state = JC_BRAIN_READY;
	}
	// Ready, but not necessarily ready to be SEEN to be ready.
	return brain->state == JC_BRAIN_READY && brain->waited >= JC_THINK_FLOOR_TICKS;
}

JcMove jc_brain_take(JcBrain *brain) {
	const JcMove move = brain->move;
	brain->state = JC_BRAIN_WAITING;
	brain->move = jc_move_none();
	return move;
}

void jc_brain_abandon(JcBrain *brain) {
	if (brain->state == JC_BRAIN_THINKING && brain->threaded && brain->worker) {
		// Raise the flag the search asks about, then wait. The worker notices
		// within a few thousand positions — a fraction of a millisecond —
		// which is why starting a new game never waits out a search whose
		// answer nobody wants.
		jet_mutex_lock(brain->lock);
		brain->abandoning = true;
		jet_mutex_unlock(brain->lock);
		jet_thread_join(brain->worker);
		brain->worker = NULL;
	}
	brain->abandoning = false;
	brain->state = JC_BRAIN_WAITING;
	brain->finished = false;
	brain->move = jc_move_none();
}
